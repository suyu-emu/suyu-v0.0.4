// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <random>
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/arm/dynarmic/arm_dynarmic.h"
#include "core/arm/dynarmic/dynarmic_exclusive_monitor.h"
#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/kernel/k_shared_memory_info.h"
#include "core/hle/kernel/k_thread_local_page.h"
#include "core/hle/kernel/k_thread_queue.h"
#include "core/hle/kernel/k_worker_task_manager.h"

#include "core/arm/dynarmic/arm_dynarmic_32.h"
#include "core/arm/dynarmic/arm_dynarmic_64.h"
#ifdef HAS_NCE
#include "core/arm/nce/arm_nce.h"
#endif

namespace Kernel {

namespace {

Result TerminateChildren(KernelCore& kernel, KProcess* process, const KThread* thread_to_not_terminate) {
    // Request that all children threads terminate.
    {
        KScopedLightLock proc_lk(process->GetListLock());
        KScopedSchedulerLock sl(kernel);

        if (thread_to_not_terminate != nullptr &&
            process->GetPinnedThread(GetCurrentCoreId(kernel)) == thread_to_not_terminate) {
            // NOTE: Here Nintendo unpins the current thread instead of the thread_to_not_terminate.
            // This is valid because the only caller which uses non-nullptr as argument uses
            // GetCurrentThreadPointer(), but it's still notable because it seems incorrect at
            // first glance.
            process->UnpinCurrentThread(kernel);
        }

        auto& thread_list = process->GetThreadList();
        for (auto it = thread_list.begin(); it != thread_list.end(); ++it) {
            if (KThread* thread = std::addressof(*it); thread != thread_to_not_terminate) {
                if (thread->GetState() != ThreadState::Terminated) {
                    thread->RequestTerminate(kernel);
                }
            }
        }
    }

    // Wait for all children threads to terminate.
    while (true) {
        // Get the next child.
        KThread* cur_child = nullptr;
        {
            KScopedLightLock proc_lk(process->GetListLock());

            auto& thread_list = process->GetThreadList();
            for (auto it = thread_list.begin(); it != thread_list.end(); ++it) {
                if (KThread* thread = std::addressof(*it); thread != thread_to_not_terminate) {
                    if (thread->GetState() != ThreadState::Terminated) {
                        if (thread->Open(kernel)) {
                            cur_child = thread;
                            break;
                        }
                    }
                }
            }
        }

        // If we didn't find any non-terminated children, we're done.
        if (cur_child == nullptr) {
            break;
        }

        // Terminate and close the thread.
        SCOPE_EXIT {
            cur_child->Close(kernel);
        };

        if (const Result terminate_result = cur_child->Terminate(kernel); ResultTerminationRequested == terminate_result) {
            R_THROW(terminate_result);
        }
    }

    R_SUCCEED();
}

class ThreadQueueImplForKProcessEnterUserException final : public KThreadQueue {
private:
    KThread** m_exception_thread;

public:
    explicit ThreadQueueImplForKProcessEnterUserException(KernelCore& kernel, KThread** t)
        : KThreadQueue(kernel), m_exception_thread(t) {}

    virtual void EndWait(KernelCore& kernel, KThread* waiting_thread, Result wait_result) override {
        // Set the exception thread.
        *m_exception_thread = waiting_thread;

        // Invoke the base end wait handler.
        KThreadQueue::EndWait(kernel, waiting_thread, wait_result);
    }

    virtual void CancelWait(KernelCore& kernel, KThread* waiting_thread, Result wait_result,
                            bool cancel_timer_task) override {
        // Remove the thread as a waiter on its mutex owner.
        waiting_thread->GetLockOwner(kernel)->RemoveWaiter(kernel, waiting_thread);

        // Invoke the base cancel wait handler.
        KThreadQueue::CancelWait(kernel, waiting_thread, wait_result, cancel_timer_task);
    }
};

void GenerateRandom(std::span<u64> out_random) {
    std::mt19937 rng(Settings::values.rng_seed_enabled ? Settings::values.rng_seed.GetValue()
                                                       : static_cast<u32>(std::time(nullptr)));
    std::uniform_int_distribution<u64> distribution;
    std::generate(out_random.begin(), out_random.end(), [&] { return distribution(rng); });
}

} // namespace

void KProcess::Finalize(KernelCore& kernel) {
    // Delete the process local region.
    this->DeleteThreadLocalRegion(kernel, m_plr_address);

    // Get the used memory size.
    const size_t used_memory_size = this->GetUsedNonSystemUserPhysicalMemorySize(kernel);

    // Finalize the page table.
    m_page_table.Finalize();

    // Finish using our system resource.
    if (m_system_resource) {
        if (m_system_resource->IsSecureResource()) {
            // Finalize optimized memory. If memory wasn't optimized, this is a no-op.
            kernel.MemoryManager().FinalizeOptimizedMemory(this->GetId(), m_memory_pool);
        }

        m_system_resource->Close(kernel);
        m_system_resource = nullptr;
    }

    // Free all shared memory infos.
    {
        auto it = m_shared_memory_list.begin();
        while (it != m_shared_memory_list.end()) {
            KSharedMemoryInfo* info = std::addressof(*it);
            KSharedMemory* shmem = info->GetSharedMemory();

            while (!info->Close()) {
                shmem->Close(kernel);
            }
            shmem->Close(kernel);

            it = m_shared_memory_list.erase(it);
            KSharedMemoryInfo::Free(kernel, info);
        }
    }

    // Our thread local page list must be empty at this point.
    ASSERT(m_partially_used_tlp_tree.empty());
    ASSERT(m_fully_used_tlp_tree.empty());

    // Release memory to the resource limit.
    if (m_resource_limit != nullptr) {
        ASSERT(used_memory_size >= m_memory_release_hint);
        m_resource_limit->Release(kernel, Svc::LimitableResource::PhysicalMemoryMax, used_memory_size, used_memory_size - m_memory_release_hint);
        m_resource_limit->Close(kernel);
    }

    // Clear expensive resources, as the destructor is not called for guest objects.
    for (auto& interface : m_arm_interfaces) {
        interface.reset();
    }
    m_exclusive_monitor.reset();

    // Perform inherited finalization.
    KSynchronizationObject::Finalize(kernel);
}

Result KProcess::Initialize(KernelCore& kernel, const Svc::CreateProcessParameter& params, KResourceLimit* res_limit, bool is_real) {
    // TODO: remove this special case
    if (is_real) {
        // Create and clear the process local region.
        R_TRY(this->CreateThreadLocalRegion(kernel, std::addressof(m_plr_address)));
        this->GetMemory().ZeroBlock(m_plr_address, Svc::ThreadLocalRegionSize);
    }

    // Copy in the name from parameters.
    static_assert(sizeof(params.name) < sizeof(m_name));
    std::memcpy(m_name.data(), params.name.data(), sizeof(params.name));
    m_name[sizeof(params.name)] = 0;

    // Set misc fields.
    m_state = State::Created;
    m_main_thread_stack_size = 0;
    m_used_kernel_memory_size = 0;
    m_ideal_core_id = 0;
    m_flags = params.flags;
    m_version = params.version;
    m_program_id = params.program_id;
    m_code_address = params.code_address;
    m_arg_pointer = 0;
    m_arg_return_address = 0;
    m_main_thread_handle_addr = 0;
    m_code_size = params.code_num_pages * PageSize;
    m_is_application = True(params.flags & Svc::CreateProcessFlag::IsApplication);

    // Set thread fields.
    for (size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
        m_running_threads[i] = nullptr;
        m_pinned_threads[i] = nullptr;
        m_running_thread_idle_counts[i] = 0;
        m_running_thread_switch_counts[i] = 0;
    }

    // Set max memory based on address space type.
    switch ((params.flags & Svc::CreateProcessFlag::AddressSpaceMask)) {
    case Svc::CreateProcessFlag::AddressSpace32Bit:
    case Svc::CreateProcessFlag::AddressSpace64BitDeprecated:
    case Svc::CreateProcessFlag::AddressSpace64Bit:
        m_max_process_memory = m_page_table.GetHeapRegionSize();
        break;
    case Svc::CreateProcessFlag::AddressSpace32BitWithoutAlias:
        m_max_process_memory = m_page_table.GetHeapRegionSize() + m_page_table.GetAliasRegionSize();
        break;
    default:
        UNREACHABLE();
    }

    // Generate random entropy.
    GenerateRandom(m_entropy);

    // Clear remaining fields.
    m_num_running_threads = 0;
    m_num_process_switches = 0;
    m_num_thread_switches = 0;
    m_num_fpu_switches = 0;
    m_num_supervisor_calls = 0;
    m_num_ipc_messages = 0;

    m_is_signaled = false;
    m_exception_thread = nullptr;
    m_is_suspended = false;
    m_memory_release_hint = 0;
    m_schedule_count = 0;
    m_is_handle_table_initialized = false;

    // Open a reference to our resource limit.
    m_resource_limit = res_limit;
    m_resource_limit->Open(kernel);

    // We're initialized!
    m_is_initialized = true;

    R_SUCCEED();
}

Result KProcess::Initialize(KernelCore& kernel, const Svc::CreateProcessParameter& params, const KPageGroup& pg,
                            std::span<const u32> caps, KResourceLimit* res_limit,
                            KMemoryManager::Pool pool, bool immortal) {
    ASSERT(res_limit != nullptr);
    ASSERT((params.code_num_pages * PageSize) / PageSize ==
           static_cast<size_t>(params.code_num_pages));

    // Set members.
    m_memory_pool = pool;
    m_is_default_application_system_resource = false;
    m_is_immortal = immortal;

    // Setup our system resource.
    if (const size_t system_resource_num_pages = params.system_resource_num_pages;
        system_resource_num_pages != 0) {
        // Create a secure system resource.
        KSecureSystemResource* secure_resource = KSecureSystemResource::Create(kernel);
        R_UNLESS(secure_resource != nullptr, ResultOutOfResource);

        ON_RESULT_FAILURE {
            secure_resource->Close(kernel);
        };

        // Initialize the secure resource.
        R_TRY(secure_resource->Initialize(kernel, system_resource_num_pages * PageSize, res_limit, m_memory_pool));

        // Set our system resource.
        m_system_resource = secure_resource;
    } else {
        // Use the system-wide system resource.
        const bool is_app = True(params.flags & Svc::CreateProcessFlag::IsApplication);
        m_system_resource = std::addressof(is_app ? kernel.GetAppSystemResource()
                                                  : kernel.GetSystemSystemResource());

        m_is_default_application_system_resource = is_app;

        // Open reference to the system resource.
        m_system_resource->Open(kernel);
    }

    // Ensure we clean up our secure resource, if we fail.
    ON_RESULT_FAILURE {
        m_system_resource->Close(kernel);
        m_system_resource = nullptr;
    };

    // Setup page table.
    {
        const auto as_type = params.flags & Svc::CreateProcessFlag::AddressSpaceMask;
        const bool enable_aslr = True(params.flags & Svc::CreateProcessFlag::EnableAslr);
        const bool enable_das_merge =
            False(params.flags & Svc::CreateProcessFlag::DisableDeviceAddressSpaceMerge);
        R_TRY(m_page_table.Initialize(as_type, enable_aslr, enable_das_merge, !enable_aslr, pool,
                                      params.code_address, params.code_num_pages * PageSize,
                                      m_system_resource, res_limit, m_memory, 0));
    }
    ON_RESULT_FAILURE_2 {
        m_page_table.Finalize();
    };

    // Ensure our memory is initialized.
    m_memory.SetCurrentPageTable(*this);
    m_memory.SetGPUDirtyManagers(kernel.System().GetGPUDirtyMemoryManager());

    // Ensure we can insert the code region.
    R_UNLESS(m_page_table.CanContain(params.code_address, params.code_num_pages * PageSize,
                                     KMemoryState::Code),
             ResultInvalidMemoryRegion);

    // Map the code region.
    R_TRY(m_page_table.MapPageGroup(params.code_address, pg, KMemoryState::Code,
                                    KMemoryPermission::KernelRead));

    // Initialize capabilities.
    R_TRY(m_capabilities.InitializeForKip(caps, std::addressof(m_page_table)));

    // Enable mapping device pages as executable on legacy processes.
    if (m_capabilities.GetIntendedKernelMajorVersion() < 26) {
        m_page_table.AllowDeviceMappingOfExecPages();
    }

    // Initialize the process id.
    m_process_id = kernel.CreateNewUserProcessID();
    ASSERT(InitialProcessIdMin <= m_process_id);
    ASSERT(m_process_id <= InitialProcessIdMax);

    // Initialize the rest of the process.
    R_TRY(this->Initialize(kernel, params, res_limit, true));

    // We succeeded!
    R_SUCCEED();
}

Result KProcess::Initialize(KernelCore& kernel, const Svc::CreateProcessParameter& params,
                            std::span<const u32> user_caps, KResourceLimit* res_limit,
                            KMemoryManager::Pool pool, KProcessAddress aslr_space_start) {
    ASSERT(res_limit != nullptr);

    // Set members.
    m_memory_pool = pool;
    m_is_default_application_system_resource = false;
    m_is_immortal = false;

    // Get the memory sizes.
    const size_t code_num_pages = params.code_num_pages;
    const size_t system_resource_num_pages = params.system_resource_num_pages;
    const size_t code_size = code_num_pages * PageSize;
    const size_t system_resource_size = system_resource_num_pages * PageSize;

    // Reserve memory for our code resource.
    KScopedResourceReservation memory_reservation(kernel,
        res_limit, Svc::LimitableResource::PhysicalMemoryMax, code_size);
    R_UNLESS(memory_reservation.Succeeded(), ResultLimitReached);

    // Setup our system resource.
    if (system_resource_num_pages != 0) {
        // Create a secure system resource.
        KSecureSystemResource* secure_resource = KSecureSystemResource::Create(kernel);
        R_UNLESS(secure_resource != nullptr, ResultOutOfResource);

        ON_RESULT_FAILURE {
            secure_resource->Close(kernel);
        };

        // Initialize the secure resource.
        R_TRY(secure_resource->Initialize(kernel, system_resource_size, res_limit, m_memory_pool));

        // Set our system resource.
        m_system_resource = secure_resource;

    } else {
        // Use the system-wide system resource.
        const bool is_app = True(params.flags & Svc::CreateProcessFlag::IsApplication);
        m_system_resource = std::addressof(is_app ? kernel.GetAppSystemResource()
                                                  : kernel.GetSystemSystemResource());

        m_is_default_application_system_resource = is_app;

        // Open reference to the system resource.
        m_system_resource->Open(kernel);
    }

    // Ensure we clean up our secure resource, if we fail.
    ON_RESULT_FAILURE {
        m_system_resource->Close(kernel);
        m_system_resource = nullptr;
    };

    // Setup page table.
    {
        const auto as_type = params.flags & Svc::CreateProcessFlag::AddressSpaceMask;
        const bool enable_aslr = True(params.flags & Svc::CreateProcessFlag::EnableAslr);
        const bool enable_das_merge =
            False(params.flags & Svc::CreateProcessFlag::DisableDeviceAddressSpaceMerge);
        R_TRY(m_page_table.Initialize(as_type, enable_aslr, enable_das_merge, !enable_aslr, pool,
                                      params.code_address, code_size, m_system_resource, res_limit,
                                      m_memory, aslr_space_start));
    }
    ON_RESULT_FAILURE_2 {
        m_page_table.Finalize();
    };

    // Ensure our memory is initialized.
    m_memory.SetCurrentPageTable(*this);
    m_memory.SetGPUDirtyManagers(kernel.System().GetGPUDirtyMemoryManager());

    // Ensure we can insert the code region.
    R_UNLESS(m_page_table.CanContain(params.code_address, code_size, KMemoryState::Code),
             ResultInvalidMemoryRegion);

    // Map the code region.
    R_TRY(m_page_table.MapPages(params.code_address, code_num_pages, KMemoryState::Code,
                                KMemoryPermission::KernelRead | KMemoryPermission::NotMapped));

    // Initialize capabilities.
    R_TRY(m_capabilities.InitializeForUser(user_caps, std::addressof(m_page_table)));

    // Enable mapping device pages as executable on legacy processes.
    if (m_capabilities.GetIntendedKernelMajorVersion() < 26) {
        m_page_table.AllowDeviceMappingOfExecPages();
    }

    // Initialize the process id.
    m_process_id = kernel.CreateNewUserProcessID();
    ASSERT(ProcessIdMin <= m_process_id);
    ASSERT(m_process_id <= ProcessIdMax);

    // If we should optimize memory allocations, do so.
    if (m_system_resource->IsSecureResource() &&
        True(params.flags & Svc::CreateProcessFlag::OptimizeMemoryAllocation)) {
        R_TRY(kernel.MemoryManager().InitializeOptimizedMemory(m_process_id, pool));
    }

    // Initialize the rest of the process.
    R_TRY(this->Initialize(kernel, params, res_limit, true));

    // We succeeded, so commit our memory reservation.
    memory_reservation.Commit();
    R_SUCCEED();
}

void KProcess::DoWorkerTaskImpl(KernelCore& kernel) {
    // Terminate child threads.
    TerminateChildren(kernel, this, nullptr);

    // Finalize the handle table, if we're not immortal.
    if (!m_is_immortal && m_is_handle_table_initialized) {
        this->FinalizeHandleTable(kernel);
    }

    // Finish termination.
    this->FinishTermination(kernel);
}

Result KProcess::StartTermination(KernelCore& kernel) {
    // Finalize the handle table when we're done, if the process isn't immortal.
    SCOPE_EXIT {
        if (!m_is_immortal) {
            this->FinalizeHandleTable(kernel);
        }
    };

    // Terminate child threads other than the current one.
    R_RETURN(TerminateChildren(kernel, this, GetCurrentThreadPointer(kernel)));
}

void KProcess::FinishTermination(KernelCore& kernel) {
    // Only allow termination to occur if the process isn't immortal.
    if (!m_is_immortal) {
        // Release resource limit hint.
        if (m_resource_limit != nullptr) {
            m_memory_release_hint = this->GetUsedNonSystemUserPhysicalMemorySize(kernel);
            m_resource_limit->Release(kernel, Svc::LimitableResource::PhysicalMemoryMax, 0, m_memory_release_hint);
        }

        // Change state.
        {
            KScopedSchedulerLock sl(kernel);
            this->ChangeState(kernel, State::Terminated);
        }

        // Close.
        this->Close(kernel);
    }
}

void KProcess::Exit(KernelCore& kernel) {
    // Determine whether we need to start terminating
    bool needs_terminate = false;
    {
        KScopedLightLock lk(m_state_lock);
        KScopedSchedulerLock sl(kernel);

        ASSERT(m_state != State::Created);
        ASSERT(m_state != State::CreatedAttached);
        ASSERT(m_state != State::Crashed);
        ASSERT(m_state != State::Terminated);
        if (m_state == State::Running || m_state == State::RunningAttached || m_state == State::DebugBreak) {
            this->ChangeState(kernel, State::Terminating);
            needs_terminate = true;
        }
    }

    // If we need to start termination, do so.
    if (needs_terminate) {
        this->StartTermination(kernel);

        // Register the process as a work task.
        kernel.WorkerTaskManager().AddTask(kernel, KWorkerTaskManager::WorkerType::Exit, this);
    }

    // Exit the current thread.
    GetCurrentThread(kernel).Exit(kernel);
}

Result KProcess::Terminate(KernelCore& kernel) {
    // Determine whether we need to start terminating.
    bool needs_terminate = false;
    {
        KScopedLightLock lk(m_state_lock);

        // Check whether we're allowed to terminate.
        R_UNLESS(m_state != State::Created, ResultInvalidState);
        R_UNLESS(m_state != State::CreatedAttached, ResultInvalidState);

        KScopedSchedulerLock sl(kernel);

        if (m_state == State::Running || m_state == State::RunningAttached ||
            m_state == State::Crashed || m_state == State::DebugBreak) {
            this->ChangeState(kernel, State::Terminating);
            needs_terminate = true;
        }
    }

    // If we need to terminate, do so.
    if (needs_terminate) {
        // Start termination.
        if (R_SUCCEEDED(this->StartTermination(kernel))) {
            // Finish termination.
            this->FinishTermination(kernel);
        } else {
            // Register the process as a work task.
            kernel.WorkerTaskManager().AddTask(kernel, KWorkerTaskManager::WorkerType::Exit, this);
        }
    }

    R_SUCCEED();
}

Result KProcess::AddSharedMemory(KernelCore& kernel, KSharedMemory* shmem, KProcessAddress address, size_t size) {
    // Lock ourselves, to prevent concurrent access.
    KScopedLightLock lk(m_state_lock);

    // Try to find an existing info for the memory.
    KSharedMemoryInfo* info = nullptr;
    for (auto it = m_shared_memory_list.begin(); it != m_shared_memory_list.end(); ++it) {
        if (it->GetSharedMemory() == shmem) {
            info = std::addressof(*it);
            break;
        }
    }

    // If we didn't find an info, create one.
    if (info == nullptr) {
        // Allocate a new info.
        info = KSharedMemoryInfo::Allocate(kernel);
        R_UNLESS(info != nullptr, ResultOutOfResource);

        // Initialize the info and add it to our list.
        info->Initialize(shmem);
        m_shared_memory_list.push_back(*info);
    }

    // Open a reference to the shared memory and its info.
    shmem->Open(kernel);
    info->Open();

    R_SUCCEED();
}

void KProcess::RemoveSharedMemory(KernelCore& kernel, KSharedMemory* shmem, KProcessAddress address, size_t size) {
    // Lock ourselves, to prevent concurrent access.
    KScopedLightLock lk(m_state_lock);

    // Find an existing info for the memory.
    KSharedMemoryInfo* info = nullptr;
    auto it = m_shared_memory_list.begin();
    for (; it != m_shared_memory_list.end(); ++it) {
        if (it->GetSharedMemory() == shmem) {
            info = std::addressof(*it);
            break;
        }
    }
    ASSERT(info != nullptr);

    // Close a reference to the info and its memory.
    if (info->Close()) {
        m_shared_memory_list.erase(it);
        KSharedMemoryInfo::Free(kernel, info);
    }

    shmem->Close(kernel);
}

Result KProcess::CreateThreadLocalRegion(KernelCore& kernel, KProcessAddress* out) {
    KThreadLocalPage* tlp = nullptr;
    KProcessAddress tlr = 0;

    // See if we can get a region from a partially used TLP.
    {
        KScopedSchedulerLock sl(kernel);

        if (auto it = m_partially_used_tlp_tree.begin(); it != m_partially_used_tlp_tree.end()) {
            tlr = it->Reserve();
            ASSERT(tlr != 0);

            if (it->IsAllUsed()) {
                tlp = std::addressof(*it);
                m_partially_used_tlp_tree.erase(it);
                m_fully_used_tlp_tree.insert(*tlp);
            }

            *out = tlr;
            R_SUCCEED();
        }
    }

    // Allocate a new page.
    tlp = KThreadLocalPage::Allocate(kernel);
    R_UNLESS(tlp != nullptr, ResultOutOfMemory);
    ON_RESULT_FAILURE {
        KThreadLocalPage::Free(kernel, tlp);
    };

    // Initialize the new page.
    R_TRY(tlp->Initialize(kernel, this));

    // Reserve a TLR.
    tlr = tlp->Reserve();
    ASSERT(tlr != 0);

    // Insert into our tree.
    {
        KScopedSchedulerLock sl(kernel);
        if (tlp->IsAllUsed()) {
            m_fully_used_tlp_tree.insert(*tlp);
        } else {
            m_partially_used_tlp_tree.insert(*tlp);
        }
    }

    // We succeeded!
    *out = tlr;
    R_SUCCEED();
}

Result KProcess::DeleteThreadLocalRegion(KernelCore& kernel, KProcessAddress addr) {
    KThreadLocalPage* page_to_free = nullptr;

    // Release the region.
    {
        KScopedSchedulerLock sl(kernel);

        // Try to find the page in the partially used list.
        auto it = m_partially_used_tlp_tree.find_key(Common::AlignDown(GetInteger(addr), PageSize));
        if (it == m_partially_used_tlp_tree.end()) {
            // If we don't find it, it has to be in the fully used list.
            it = m_fully_used_tlp_tree.find_key(Common::AlignDown(GetInteger(addr), PageSize));
            R_UNLESS(it != m_fully_used_tlp_tree.end(), ResultInvalidAddress);

            // Release the region.
            it->Release(addr);

            // Move the page out of the fully used list.
            KThreadLocalPage* tlp = std::addressof(*it);
            m_fully_used_tlp_tree.erase(it);
            if (tlp->IsAllFree()) {
                page_to_free = tlp;
            } else {
                m_partially_used_tlp_tree.insert(*tlp);
            }
        } else {
            // Release the region.
            it->Release(addr);

            // Handle the all-free case.
            KThreadLocalPage* tlp = std::addressof(*it);
            if (tlp->IsAllFree()) {
                m_partially_used_tlp_tree.erase(it);
                page_to_free = tlp;
            }
        }
    }

    // If we should free the page it was in, do so.
    if (page_to_free != nullptr) {
        page_to_free->Finalize();

        KThreadLocalPage::Free(kernel, page_to_free);
    }

    R_SUCCEED();
}

bool KProcess::ReserveResource(KernelCore& kernel, Svc::LimitableResource which, s64 value) {
    if (KResourceLimit* rl = this->GetResourceLimit(); rl != nullptr) {
        return rl->Reserve(kernel, which, value);
    } else {
        return true;
    }
}

bool KProcess::ReserveResource(KernelCore& kernel, Svc::LimitableResource which, s64 value, s64 timeout) {
    if (KResourceLimit* rl = this->GetResourceLimit(); rl != nullptr) {
        return rl->Reserve(kernel, which, value, timeout);
    } else {
        return true;
    }
}

void KProcess::ReleaseResource(KernelCore& kernel, Svc::LimitableResource which, s64 value) {
    if (KResourceLimit* rl = this->GetResourceLimit(); rl != nullptr) {
        rl->Release(kernel, which, value);
    }
}

void KProcess::ReleaseResource(KernelCore& kernel, Svc::LimitableResource which, s64 value, s64 hint) {
    if (KResourceLimit* rl = this->GetResourceLimit(); rl != nullptr) {
        rl->Release(kernel, which, value, hint);
    }
}

void KProcess::IncrementRunningThreadCount(KernelCore& kernel) {
    ASSERT(m_num_running_threads.load() >= 0);

    ++m_num_running_threads;
}

void KProcess::DecrementRunningThreadCount(KernelCore& kernel) {
    ASSERT(m_num_running_threads.load() > 0);
    if (const auto prev = m_num_running_threads--; prev == 1) {
        this->Terminate(kernel);
    }
}

bool KProcess::EnterUserException(KernelCore& kernel) {
    // Get the current thread.
    KThread* cur_thread = GetCurrentThreadPointer(kernel);
    ASSERT(this == cur_thread->GetOwnerProcess());

    // Check that we haven't already claimed the exception thread.
    if (m_exception_thread == cur_thread) {
        return false;
    }

    // Create the wait queue we'll be using.
    ThreadQueueImplForKProcessEnterUserException wait_queue(kernel,
                                                            std::addressof(m_exception_thread));

    // Claim the exception thread.
    {
        // Lock the scheduler.
        KScopedSchedulerLock sl(kernel);

        // Check that we're not terminating.
        if (cur_thread->IsTerminationRequested()) {
            return false;
        }

        // If we don't have an exception thread, we can just claim it directly.
        if (m_exception_thread == nullptr) {
            m_exception_thread = cur_thread;
            KScheduler::SetSchedulerUpdateNeeded(kernel);
            return true;
        }

        // Otherwise, we need to wait until we don't have an exception thread.

        // Add the current thread as a waiter on the current exception thread.
        cur_thread->SetKernelAddressKey(uintptr_t(std::addressof(m_exception_thread)) | 1);
        m_exception_thread->AddWaiter(kernel, cur_thread);

        // Wait to claim the exception thread.
        cur_thread->BeginWait(kernel, std::addressof(wait_queue));
    }

    // If our wait didn't end due to thread termination, we succeeded.
    return ResultTerminationRequested != cur_thread->GetWaitResult();
}

bool KProcess::LeaveUserException(KernelCore& kernel) {
    return this->ReleaseUserException(kernel, GetCurrentThreadPointer(kernel));
}

bool KProcess::ReleaseUserException(KernelCore& kernel, KThread* thread) {
    KScopedSchedulerLock sl(kernel);

    if (m_exception_thread == thread) {
        m_exception_thread = nullptr;

        // Remove waiter thread.
        bool has_waiters;
        if (KThread* next = thread->RemoveKernelWaiterByKey(kernel,
                std::addressof(has_waiters),
                reinterpret_cast<uintptr_t>(std::addressof(m_exception_thread)) | 1);
            next != nullptr) {
            next->EndWait(kernel, ResultSuccess);
        }

        KScheduler::SetSchedulerUpdateNeeded(kernel);

        return true;
    } else {
        return false;
    }
}

void KProcess::RegisterThread(KernelCore& kernel, KThread* thread) {
    KScopedLightLock lk(m_list_lock);

    m_thread_list.push_back(*thread);
}

void KProcess::UnregisterThread(KernelCore& kernel, KThread* thread) {
    KScopedLightLock lk(m_list_lock);

    m_thread_list.erase(m_thread_list.iterator_to(*thread));
}

size_t KProcess::GetUsedUserPhysicalMemorySize(KernelCore& kernel) const {
    const size_t norm_size = m_page_table.GetNormalMemorySize();
    const size_t other_size = m_code_size + m_main_thread_stack_size;
    const size_t sec_size = this->GetRequiredSecureMemorySizeNonDefault();

    return norm_size + other_size + sec_size;
}

size_t KProcess::GetTotalUserPhysicalMemorySize(KernelCore& kernel) const {
    // Get the amount of free and used size.
    const size_t free_size =
        m_resource_limit->GetFreeValue(Svc::LimitableResource::PhysicalMemoryMax);
    const size_t max_size = m_max_process_memory;

    // Determine used size.
    // NOTE: This does *not* check this->IsDefaultApplicationSystemResource(), unlike
    // GetUsedUserPhysicalMemorySize().
    const size_t norm_size = m_page_table.GetNormalMemorySize();
    const size_t other_size = m_code_size + m_main_thread_stack_size;
    const size_t sec_size = this->GetRequiredSecureMemorySize();
    const size_t used_size = norm_size + other_size + sec_size;

    // NOTE: These function calls will recalculate, introducing a race...it is unclear why Nintendo
    // does it this way.
    if (used_size + free_size > max_size) {
        return max_size;
    } else {
        return free_size + this->GetUsedUserPhysicalMemorySize(kernel);
    }
}

size_t KProcess::GetUsedNonSystemUserPhysicalMemorySize(KernelCore& kernel) const {
    const size_t norm_size = m_page_table.GetNormalMemorySize();
    const size_t other_size = m_code_size + m_main_thread_stack_size;

    return norm_size + other_size;
}

size_t KProcess::GetTotalNonSystemUserPhysicalMemorySize(KernelCore& kernel) const {
    // Get the amount of free and used size.
    const size_t free_size =
        m_resource_limit->GetFreeValue(Svc::LimitableResource::PhysicalMemoryMax);
    const size_t max_size = m_max_process_memory;

    // Determine used size.
    // NOTE: This does *not* check this->IsDefaultApplicationSystemResource(), unlike
    // GetUsedUserPhysicalMemorySize().
    const size_t norm_size = m_page_table.GetNormalMemorySize();
    const size_t other_size = m_code_size + m_main_thread_stack_size;
    const size_t sec_size = this->GetRequiredSecureMemorySize();
    const size_t used_size = norm_size + other_size + sec_size;

    // NOTE: These function calls will recalculate, introducing a race...it is unclear why Nintendo
    // does it this way.
    if (used_size + free_size > max_size) {
        return max_size - this->GetRequiredSecureMemorySizeNonDefault();
    } else {
        return free_size + this->GetUsedNonSystemUserPhysicalMemorySize(kernel);
    }
}

Result KProcess::Run(KernelCore& kernel, s32 priority, size_t stack_size) {
    // Lock ourselves, to prevent concurrent access.
    KScopedLightLock lk(m_state_lock);

    // Validate that we're in a state where we can initialize.
    const auto state = m_state;
    R_UNLESS(state == State::Created || state == State::CreatedAttached, ResultInvalidState);

    // Place a tentative reservation of a thread for this process.
    KScopedResourceReservation thread_reservation(kernel, this, Svc::LimitableResource::ThreadCountMax);
    R_UNLESS(thread_reservation.Succeeded(), ResultLimitReached);

    // Ensure that we haven't already allocated stack.
    ASSERT(m_main_thread_stack_size == 0);

    // Ensure that we're allocating a valid stack.
    stack_size = Common::AlignUp(stack_size, PageSize);
    R_UNLESS(stack_size + m_code_size <= m_max_process_memory, ResultOutOfMemory);
    R_UNLESS(stack_size + m_code_size >= m_code_size, ResultOutOfMemory);

    // Place a tentative reservation of memory for our new stack.
    KScopedResourceReservation mem_reservation(kernel, this, Svc::LimitableResource::PhysicalMemoryMax,
                                               stack_size);
    R_UNLESS(mem_reservation.Succeeded(), ResultLimitReached);

    // Allocate and map our stack.
    KProcessAddress stack_top = 0;
    if (stack_size) {
        KProcessAddress stack_bottom;
        R_TRY(m_page_table.MapPages(std::addressof(stack_bottom), stack_size / PageSize,
                                    KMemoryState::Stack, KMemoryPermission::UserReadWrite));

        stack_top = stack_bottom + stack_size;
        m_main_thread_stack_size = stack_size;
    }

    // Ensure our stack is safe to clean up on exit.
    ON_RESULT_FAILURE {
        if (m_main_thread_stack_size) {
            ASSERT(R_SUCCEEDED(m_page_table.UnmapPages(stack_top - m_main_thread_stack_size,
                                                       m_main_thread_stack_size / PageSize,
                                                       KMemoryState::Stack)));
            m_main_thread_stack_size = 0;
        }
    };

    // Set our maximum heap size.
    R_TRY(m_page_table.SetMaxHeapSize(m_max_process_memory -
                                      (m_main_thread_stack_size + m_code_size)));

    // Initialize our handle table.
    R_TRY(this->InitializeHandleTable(kernel, m_capabilities.GetHandleTableSize()));
    ON_RESULT_FAILURE_2 {
        this->FinalizeHandleTable(kernel);
    };

    // Create a new thread for the process.
    KThread* main_thread = KThread::Create(kernel);
    R_UNLESS(main_thread != nullptr, ResultOutOfResource);
    SCOPE_EXIT {
        main_thread->Close(kernel);
    };

    // Initialize the thread.
    R_TRY(KThread::InitializeUserThread(kernel.System(), main_thread, this->GetEntryPoint(), 0,
                                        stack_top, priority, m_ideal_core_id, this));

    // Register the thread, and commit our reservation.
    KThread::Register(kernel, main_thread);
    thread_reservation.Commit();

    // Add the thread to our handle table.
    Handle thread_handle;
    R_TRY(m_handle_table.Add(kernel, std::addressof(thread_handle), main_thread));

    // Set the thread arguments. Two distinct entry conventions:
    //  * Kernel/NSO entry  (no homebrew ABI):  x0 = 0,                x1 = thread_handle
    //  * Homebrew/NRO ABI  (loader set arg ptr): x0 = ConfigEntry ptr, x1 = -1ULL
    // libnx's switch_crt0.s tests `x0==0 || x1==0xFFFFFFFFFFFFFFFF` to take
    // its normal init path; any other combination is interpreted as a user
    // exception handler entry.
    if (GetInteger(m_arg_pointer) != 0) {
        main_thread->GetContext().r[0] = GetInteger(m_arg_pointer);
        main_thread->GetContext().r[1] = UINT64_MAX;
        main_thread->GetContext().lr = GetInteger(m_arg_return_address);
        // Patch the MainThreadHandle entry in the ConfigEntry table now that
        // the actual handle exists. libnx stores this verbatim and uses it
        // for thread-control SVCs later; a pseudo-handle wouldn't survive
        // svcCloseHandle on exit.
        if (GetInteger(m_main_thread_handle_addr) != 0) {
            this->GetMemory().Write32(m_main_thread_handle_addr, thread_handle);
        }
    } else {
        main_thread->GetContext().r[0] = 0;
        main_thread->GetContext().r[1] = thread_handle;
    }

    // Pass the thread handle to the thread local region.
    this->GetMemory().Write32(GetInteger(main_thread->GetTlsAddress()) + 0x110, thread_handle);

    // Update our state.
    this->ChangeState(kernel, (state == State::Created) ? State::Running : State::RunningAttached);
    ON_RESULT_FAILURE_2 {
        this->ChangeState(kernel, state);
    };

    // Suspend for debug, if we should.
    if (kernel.System().DebuggerEnabled()) {
        main_thread->RequestSuspend(kernel, SuspendType::Debug);
    }

    // Run our thread.
    R_TRY(main_thread->Run(kernel));

    // Open a reference to represent that we're running.
    this->Open(kernel);

    // We succeeded! Commit our memory reservation.
    mem_reservation.Commit();

    R_SUCCEED();
}

Result KProcess::Reset(KernelCore& kernel) {
    // Lock the process and the scheduler.
    KScopedLightLock lk(m_state_lock);
    KScopedSchedulerLock sl(kernel);

    // Validate that we're in a state that we can reset.
    R_UNLESS(m_state != State::Terminated, ResultInvalidState);
    R_UNLESS(m_is_signaled, ResultInvalidState);

    // Clear signaled.
    m_is_signaled = false;
    R_SUCCEED();
}

Result KProcess::SetActivity(KernelCore& kernel, Svc::ProcessActivity activity) {
    // Lock ourselves and the scheduler.
    KScopedLightLock lk(m_state_lock);
    KScopedLightLock list_lk(m_list_lock);
    KScopedSchedulerLock sl(kernel);

    // Validate our state.
    R_UNLESS(m_state != State::Terminating, ResultInvalidState);
    R_UNLESS(m_state != State::Terminated, ResultInvalidState);

    // Either pause or resume.
    if (activity == Svc::ProcessActivity::Paused) {
        // Verify that we're not suspended.
        R_UNLESS(!m_is_suspended, ResultInvalidState);

        // Suspend all threads.
        auto end = this->GetThreadList().end();
        for (auto it = this->GetThreadList().begin(); it != end; ++it) {
            it->RequestSuspend(kernel, SuspendType::Process);
        }

        // Set ourselves as suspended.
        this->SetSuspended(true);
    } else {
        ASSERT(activity == Svc::ProcessActivity::Runnable);

        // Verify that we're suspended.
        R_UNLESS(m_is_suspended, ResultInvalidState);

        // Resume all threads.
        auto end = this->GetThreadList().end();
        for (auto it = this->GetThreadList().begin(); it != end; ++it) {
            it->Resume(kernel, SuspendType::Process);
        }

        // Set ourselves as resumed.
        this->SetSuspended(false);
    }

    R_SUCCEED();
}

void KProcess::PinCurrentThread(KernelCore& kernel) {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(kernel));

    // Get the current thread.
    const s32 core_id = GetCurrentCoreId(kernel);
    KThread* cur_thread = GetCurrentThreadPointer(kernel);

    // If the thread isn't terminated, pin it.
    if (!cur_thread->IsTerminationRequested()) {
        // Pin it.
        this->PinThread(core_id, cur_thread);
        cur_thread->Pin(kernel, core_id);

        // An update is needed.
        KScheduler::SetSchedulerUpdateNeeded(kernel);
    }
}

void KProcess::UnpinCurrentThread(KernelCore& kernel) {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(kernel));

    // Get the current thread.
    const s32 core_id = GetCurrentCoreId(kernel);
    KThread* cur_thread = GetCurrentThreadPointer(kernel);

    // Unpin it.
    cur_thread->Unpin(kernel);
    this->UnpinThread(core_id, cur_thread);

    // An update is needed.
    KScheduler::SetSchedulerUpdateNeeded(kernel);
}

void KProcess::UnpinThread(KernelCore& kernel, KThread* thread) {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(kernel));

    // Get the thread's core id.
    const auto core_id = thread->GetActiveCore();

    // Unpin it.
    this->UnpinThread(core_id, thread);
    thread->Unpin(kernel);

    // An update is needed.
    KScheduler::SetSchedulerUpdateNeeded(kernel);
}

Result KProcess::GetThreadList(KernelCore& kernel, s32* out_num_threads, KProcessAddress out_thread_ids,
                               s32 max_out_count) {
    auto& memory = this->GetMemory();

    // Lock the list.
    KScopedLightLock lk(m_list_lock);

    // Iterate over the list.
    s32 count = 0;
    auto end = this->GetThreadList().end();
    for (auto it = this->GetThreadList().begin(); it != end; ++it) {
        // If we're within array bounds, write the id.
        if (count < max_out_count) {
            // Get the thread id.
            KThread* thread = std::addressof(*it);
            const u64 id = thread->GetId();

            // Copy the id to userland.
            memory.Write64(out_thread_ids + count * sizeof(u64), id);
        }

        // Increment the count.
        ++count;
    }

    // We successfully iterated the list.
    *out_num_threads = count;
    R_SUCCEED();
}

void KProcess::Switch(KernelCore& kernel, KProcess* cur_process, KProcess* next_process) {}

KProcess::KProcess(KernelCore& kernel)
    : KAutoObjectWithSlabHeapAndContainer(kernel)
    , m_exclusive_monitor{}
    , m_memory{kernel.System()}
    , m_handle_table{kernel}
    , m_page_table{kernel}
    , m_state_lock{kernel}
    , m_list_lock{kernel}
    , m_cond_var{kernel.System()}
    , m_address_arbiter{kernel.System()}
{}

KProcess::~KProcess() = default;

Result KProcess::LoadFromMetadata(KernelCore& kernel, const FileSys::ProgramMetadata& metadata, std::size_t code_size, KProcessAddress aslr_space_start, size_t aslr_space_offset) {
    // Create a resource limit for the process.
    const auto pool = static_cast<KMemoryManager::Pool>(metadata.GetPoolPartition());
    const auto physical_memory_size = kernel.MemoryManager().GetSize(pool);
    auto* res_limit =
        Kernel::CreateResourceLimitForProcess(kernel.System(), physical_memory_size);

    // Ensure we maintain a clean state on exit.
    SCOPE_EXIT {
        res_limit->Close(kernel);
    };

    // Declare flags and code address.
    Svc::CreateProcessFlag flag{};
    u64 code_address{};

    // Determine if we are an application.
    if (pool == KMemoryManager::Pool::Application) {
        flag |= Svc::CreateProcessFlag::IsApplication;
        m_is_application = true;
    }

    // If we are 64-bit, create as such.
    if (metadata.Is64BitProgram()) {
        flag |= Svc::CreateProcessFlag::Is64Bit;
    }

    // Set the address space type and code address.
    switch (metadata.GetAddressSpaceType()) {
    case FileSys::ProgramAddressSpaceType::Is39Bit:
        // For 39-bit processes, the ASLR region starts at 0x800'0000 and is ~512GiB large.
        // However, some (buggy) programs/libraries like skyline incorrectly depend on the
        // existence of ASLR pages before the entry point, so we will adjust the load address
        // to point to about 2GiB into the ASLR region.
        flag |= Svc::CreateProcessFlag::AddressSpace64Bit;
        code_address = 0x8000'0000 + aslr_space_offset;
        break;
    case FileSys::ProgramAddressSpaceType::Is36Bit:
        flag |= Svc::CreateProcessFlag::AddressSpace64BitDeprecated;
        code_address = 0x800'0000 + aslr_space_offset;
        break;
    case FileSys::ProgramAddressSpaceType::Is32Bit:
        flag |= Svc::CreateProcessFlag::AddressSpace32Bit;
        code_address = 0x20'0000 + aslr_space_offset;
        break;
    case FileSys::ProgramAddressSpaceType::Is32BitNoMap:
        flag |= Svc::CreateProcessFlag::AddressSpace32BitWithoutAlias;
        code_address = 0x20'0000 + aslr_space_offset;
        break;
    }

    Svc::CreateProcessParameter params{
        .name = {},
        .version = {},
        .program_id = metadata.GetTitleID(),
        .code_address = code_address + GetInteger(aslr_space_start),
        .code_num_pages = static_cast<s32>(code_size / PageSize),
        .flags = flag,
        .reslimit = Svc::InvalidHandle,
        .system_resource_num_pages = static_cast<s32>(metadata.GetSystemResourceSize() / PageSize),
    };

    // Set the process name.
    const auto& name = metadata.GetName();
    static_assert(sizeof(params.name) <= sizeof(name));
    std::memcpy(params.name.data(), name.data(), sizeof(params.name));

    // Initialize for application process.
    R_TRY(this->Initialize(kernel, params, metadata.GetKernelCapabilities(), res_limit, pool, aslr_space_start));

    // Assign remaining properties.
    m_ideal_core_id = metadata.GetMainThreadCore();

    // Set up emulation context.
    this->InitializeInterfaces(kernel);

    // We succeeded.
    R_SUCCEED();
}

void KProcess::LoadModule(KernelCore& kernel, CodeSet code_set, KProcessAddress base_addr) {
    const auto ReprotectSegment = [&](const CodeSet::Segment& segment, Svc::MemoryPermission permission) {
        m_page_table.SetProcessMemoryPermission(segment.addr + base_addr, segment.size, permission);
    };

    this->GetMemory().WriteBlock(base_addr, code_set.memory.data(), code_set.memory.size());

    ReprotectSegment(code_set.CodeSegment(), Svc::MemoryPermission::ReadExecute);
    ReprotectSegment(code_set.RODataSegment(), Svc::MemoryPermission::Read);
    ReprotectSegment(code_set.DataSegment(), Svc::MemoryPermission::ReadWrite);

#ifdef HAS_NCE
    const auto& patch = code_set.PatchSegment();
    const auto& post_patch = code_set.PostPatchSegment();
    if (this->IsApplication() && Settings::IsNceEnabled() && patch.size != 0) {
        auto& buffer = kernel.System().DeviceMemory().buffer;
        const auto& code = code_set.CodeSegment();
        buffer.Protect(GetInteger(base_addr + code.addr), code.size,
                       Common::MemoryPermission::Read | Common::MemoryPermission::Execute);
        buffer.Protect(GetInteger(base_addr + patch.addr), patch.size,
                       Common::MemoryPermission::Read | Common::MemoryPermission::Execute);
        // Protect post-patch segment if it exists like abve
        if (post_patch.size != 0) {
            buffer.Protect(GetInteger(base_addr + post_patch.addr), post_patch.size,
                           Common::MemoryPermission::Read | Common::MemoryPermission::Execute);
        }
        ReprotectSegment(code_set.PatchSegment(), Svc::MemoryPermission::None);
        if (post_patch.size != 0) {
            ReprotectSegment(code_set.PostPatchSegment(), Svc::MemoryPermission::None);
        }
    }
#endif
}

void KProcess::InitializeInterfaces(KernelCore& kernel) {
    m_exclusive_monitor =
        Core::MakeExclusiveMonitor(this->GetMemory(), Core::Hardware::NUM_CPU_CORES);

#ifdef HAS_NCE
    if (this->IsApplication() && Settings::IsNceEnabled()) {
        for (size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++)
            m_arm_interfaces[i] = std::make_unique<Core::ArmNce>(kernel.System(), true, i);
    } else
#endif
        if (this->Is64Bit()) {
        for (size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            m_arm_interfaces[i] = std::make_unique<Core::ArmDynarmic64>(
                kernel.System(), kernel.IsMulticore(), this,
                static_cast<Core::DynarmicExclusiveMonitor&>(*m_exclusive_monitor), i);
        }
    } else {
        for (size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            m_arm_interfaces[i] = std::make_unique<Core::ArmDynarmic32>(
                kernel.System(), kernel.IsMulticore(), this,
                static_cast<Core::DynarmicExclusiveMonitor&>(*m_exclusive_monitor), i);
        }
    }
}

bool KProcess::InsertWatchpoint(KernelCore& kernel, KProcessAddress addr, u64 size, DebugWatchpointType type) {
    const auto watch{std::find_if(m_watchpoints.begin(), m_watchpoints.end(), [&](const auto& wp) {
        return wp.type == DebugWatchpointType::None;
    })};

    if (watch == m_watchpoints.end()) {
        return false;
    }

    watch->start_address = addr;
    watch->end_address = addr + size;
    watch->type = type;

    for (KProcessAddress page = Common::AlignDown(GetInteger(addr), PageSize); page < addr + size;
         page += PageSize) {
        m_debug_page_refcounts[page]++;
        this->GetMemory().MarkRegionDebug(page, PageSize, true);
    }

    return true;
}

bool KProcess::RemoveWatchpoint(KernelCore& kernel, KProcessAddress addr, u64 size, DebugWatchpointType type) {
    const auto watch{std::find_if(m_watchpoints.begin(), m_watchpoints.end(), [&](const auto& wp) {
        return wp.start_address == addr && wp.end_address == addr + size && wp.type == type;
    })};

    if (watch == m_watchpoints.end()) {
        return false;
    }

    watch->start_address = 0;
    watch->end_address = 0;
    watch->type = DebugWatchpointType::None;

    for (KProcessAddress page = Common::AlignDown(GetInteger(addr), PageSize); page < addr + size;
         page += PageSize) {
        m_debug_page_refcounts[page]--;
        if (!m_debug_page_refcounts[page]) {
            this->GetMemory().MarkRegionDebug(page, PageSize, false);
        }
    }

    return true;
}

} // namespace Kernel
