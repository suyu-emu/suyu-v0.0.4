// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <bitset>
#include <ctime>
#include <memory>
#include <random>
#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/device_memory.h"
#include "core/file_sys/program_metadata.h"
#include "core/hle/kernel/code_set.h"
#include "core/hle/kernel/k_resource_limit.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory/memory_block_manager.h"
#include "core/hle/kernel/memory/page_table.h"
#include "core/hle/kernel/memory/slab_heap.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/lock.h"
#include "core/memory.h"
#include "core/settings.h"

namespace Kernel {
namespace {
/**
 * Sets up the primary application thread
 *
 * @param system The system instance to create the main thread under.
 * @param owner_process The parent process for the main thread
 * @param priority The priority to give the main thread
 */
void SetupMainThread(Core::System& system, Process& owner_process, u32 priority, VAddr stack_top) {
    const VAddr entry_point = owner_process.PageTable().GetCodeRegionStart();
    ASSERT(owner_process.GetResourceLimit()->Reserve(LimitableResource::Threads, 1));
    auto thread_res = KThread::Create(system, ThreadType::User, "main", entry_point, priority, 0,
                                      owner_process.GetIdealCoreId(), stack_top, &owner_process);

    std::shared_ptr<KThread> thread = std::move(thread_res).Unwrap();

    // Register 1 must be a handle to the main thread
    const Handle thread_handle = owner_process.GetHandleTable().Create(thread).Unwrap();
    thread->GetContext32().cpu_registers[0] = 0;
    thread->GetContext64().cpu_registers[0] = 0;
    thread->GetContext32().cpu_registers[1] = thread_handle;
    thread->GetContext64().cpu_registers[1] = thread_handle;

    auto& kernel = system.Kernel();
    // Threads by default are dormant, wake up the main thread so it runs when the scheduler fires
    {
        KScopedSchedulerLock lock{kernel};
        thread->SetState(ThreadState::Runnable);
    }
}
} // Anonymous namespace

// Represents a page used for thread-local storage.
//
// Each TLS page contains slots that may be used by processes and threads.
// Every process and thread is created with a slot in some arbitrary page
// (whichever page happens to have an available slot).
class TLSPage {
public:
    static constexpr std::size_t num_slot_entries =
        Core::Memory::PAGE_SIZE / Core::Memory::TLS_ENTRY_SIZE;

    explicit TLSPage(VAddr address) : base_address{address} {}

    bool HasAvailableSlots() const {
        return !is_slot_used.all();
    }

    VAddr GetBaseAddress() const {
        return base_address;
    }

    std::optional<VAddr> ReserveSlot() {
        for (std::size_t i = 0; i < is_slot_used.size(); i++) {
            if (is_slot_used[i]) {
                continue;
            }

            is_slot_used[i] = true;
            return base_address + (i * Core::Memory::TLS_ENTRY_SIZE);
        }

        return std::nullopt;
    }

    void ReleaseSlot(VAddr address) {
        // Ensure that all given addresses are consistent with how TLS pages
        // are intended to be used when releasing slots.
        ASSERT(IsWithinPage(address));
        ASSERT((address % Core::Memory::TLS_ENTRY_SIZE) == 0);

        const std::size_t index = (address - base_address) / Core::Memory::TLS_ENTRY_SIZE;
        is_slot_used[index] = false;
    }

private:
    bool IsWithinPage(VAddr address) const {
        return base_address <= address && address < base_address + Core::Memory::PAGE_SIZE;
    }

    VAddr base_address;
    std::bitset<num_slot_entries> is_slot_used;
};

std::shared_ptr<Process> Process::Create(Core::System& system, std::string name, ProcessType type) {
    auto& kernel = system.Kernel();

    std::shared_ptr<Process> process = std::make_shared<Process>(system);
    process->name = std::move(name);

    // TODO: This is inaccurate
    // The process should hold a reference to the kernel-wide resource limit.
    process->resource_limit = std::make_shared<KResourceLimit>(kernel, system);
    process->status = ProcessStatus::Created;
    process->program_id = 0;
    process->process_id = type == ProcessType::KernelInternal ? kernel.CreateNewKernelProcessID()
                                                              : kernel.CreateNewUserProcessID();
    process->capabilities.InitializeForMetadatalessProcess();

    std::mt19937 rng(Settings::values.rng_seed.GetValue().value_or(std::time(nullptr)));
    std::uniform_int_distribution<u64> distribution;
    std::generate(process->random_entropy.begin(), process->random_entropy.end(),
                  [&] { return distribution(rng); });

    kernel.AppendNewProcess(process);
    return process;
}

std::shared_ptr<KResourceLimit> Process::GetResourceLimit() const {
    return resource_limit;
}

void Process::IncrementThreadCount() {
    ASSERT(num_threads >= 0);
    num_created_threads++;

    if (const auto count = ++num_threads; count > peak_num_threads) {
        peak_num_threads = count;
    }
}

void Process::DecrementThreadCount() {
    ASSERT(num_threads > 0);

    if (const auto count = --num_threads; count == 0) {
        UNIMPLEMENTED_MSG("Process termination is not implemented!");
    }
}

u64 Process::GetTotalPhysicalMemoryAvailable() const {
    // TODO: This is expected to always return the application memory pool size after accurately
    // reserving kernel resources. The current workaround uses a process-local resource limit of
    // application memory pool size, which is inaccurate.
    const u64 capacity{resource_limit->GetFreeValue(LimitableResource::PhysicalMemory) +
                       page_table->GetTotalHeapSize() + GetSystemResourceSize() + image_size +
                       main_thread_stack_size};

    if (capacity < memory_usage_capacity) {
        return capacity;
    }

    return memory_usage_capacity;
}

u64 Process::GetTotalPhysicalMemoryAvailableWithoutSystemResource() const {
    return GetTotalPhysicalMemoryAvailable() - GetSystemResourceSize();
}

u64 Process::GetTotalPhysicalMemoryUsed() const {
    return image_size + main_thread_stack_size + page_table->GetTotalHeapSize() +
           GetSystemResourceSize();
}

u64 Process::GetTotalPhysicalMemoryUsedWithoutSystemResource() const {
    return GetTotalPhysicalMemoryUsed() - GetSystemResourceUsage();
}

bool Process::ReleaseUserException(KThread* thread) {
    KScopedSchedulerLock sl{kernel};

    if (exception_thread == thread) {
        exception_thread = nullptr;

        // Remove waiter thread.
        s32 num_waiters{};
        KThread* next = thread->RemoveWaiterByKey(
            std::addressof(num_waiters),
            reinterpret_cast<uintptr_t>(std::addressof(exception_thread)));
        if (next != nullptr) {
            if (next->GetState() == ThreadState::Waiting) {
                next->SetState(ThreadState::Runnable);
            } else {
                KScheduler::SetSchedulerUpdateNeeded(kernel);
            }
        }

        return true;
    } else {
        return false;
    }
}

void Process::PinCurrentThread() {
    ASSERT(kernel.GlobalSchedulerContext().IsLocked());

    // Get the current thread.
    const s32 core_id = GetCurrentCoreId(kernel);
    KThread* cur_thread = GetCurrentThreadPointer(kernel);

    // Pin it.
    PinThread(core_id, cur_thread);
    cur_thread->Pin();

    // An update is needed.
    KScheduler::SetSchedulerUpdateNeeded(kernel);
}

void Process::UnpinCurrentThread() {
    ASSERT(kernel.GlobalSchedulerContext().IsLocked());

    // Get the current thread.
    const s32 core_id = GetCurrentCoreId(kernel);
    KThread* cur_thread = GetCurrentThreadPointer(kernel);

    // Unpin it.
    cur_thread->Unpin();
    UnpinThread(core_id, cur_thread);

    // An update is needed.
    KScheduler::SetSchedulerUpdateNeeded(kernel);
}

void Process::RegisterThread(const KThread* thread) {
    thread_list.push_back(thread);
}

void Process::UnregisterThread(const KThread* thread) {
    thread_list.remove(thread);
}

ResultCode Process::Reset() {
    // Lock the process and the scheduler.
    KScopedLightLock lk(state_lock);
    KScopedSchedulerLock sl{kernel};

    // Validate that we're in a state that we can reset.
    R_UNLESS(status != ProcessStatus::Exited, ResultInvalidState);
    R_UNLESS(is_signaled, ResultInvalidState);

    // Clear signaled.
    is_signaled = false;
    return RESULT_SUCCESS;
}

ResultCode Process::LoadFromMetadata(const FileSys::ProgramMetadata& metadata,
                                     std::size_t code_size) {
    program_id = metadata.GetTitleID();
    ideal_core = metadata.GetMainThreadCore();
    is_64bit_process = metadata.Is64BitProgram();
    system_resource_size = metadata.GetSystemResourceSize();
    image_size = code_size;

    // Set initial resource limits
    resource_limit->SetLimitValue(
        LimitableResource::PhysicalMemory,
        kernel.MemoryManager().GetSize(Memory::MemoryManager::Pool::Application));
    KScopedResourceReservation memory_reservation(resource_limit, LimitableResource::PhysicalMemory,
                                                  code_size + system_resource_size);
    if (!memory_reservation.Succeeded()) {
        LOG_ERROR(Kernel, "Could not reserve process memory requirements of size {:X} bytes",
                  code_size + system_resource_size);
        return ResultResourceLimitedExceeded;
    }
    // Initialize proces address space
    if (const ResultCode result{
            page_table->InitializeForProcess(metadata.GetAddressSpaceType(), false, 0x8000000,
                                             code_size, Memory::MemoryManager::Pool::Application)};
        result.IsError()) {
        return result;
    }

    // Map process code region
    if (const ResultCode result{page_table->MapProcessCode(
            page_table->GetCodeRegionStart(), code_size / Memory::PageSize,
            Memory::MemoryState::Code, Memory::MemoryPermission::None)};
        result.IsError()) {
        return result;
    }

    // Initialize process capabilities
    const auto& caps{metadata.GetKernelCapabilities()};
    if (const ResultCode result{
            capabilities.InitializeForUserProcess(caps.data(), caps.size(), *page_table)};
        result.IsError()) {
        return result;
    }

    // Set memory usage capacity
    switch (metadata.GetAddressSpaceType()) {
    case FileSys::ProgramAddressSpaceType::Is32Bit:
    case FileSys::ProgramAddressSpaceType::Is36Bit:
    case FileSys::ProgramAddressSpaceType::Is39Bit:
        memory_usage_capacity = page_table->GetHeapRegionEnd() - page_table->GetHeapRegionStart();
        break;

    case FileSys::ProgramAddressSpaceType::Is32BitNoMap:
        memory_usage_capacity = page_table->GetHeapRegionEnd() - page_table->GetHeapRegionStart() +
                                page_table->GetAliasRegionEnd() - page_table->GetAliasRegionStart();
        break;

    default:
        UNREACHABLE();
    }

    resource_limit->SetLimitValue(LimitableResource::Threads, 608);
    resource_limit->SetLimitValue(LimitableResource::Events, 700);
    resource_limit->SetLimitValue(LimitableResource::TransferMemory, 128);
    resource_limit->SetLimitValue(LimitableResource::Sessions, 894);

    // Create TLS region
    tls_region_address = CreateTLSRegion();
    memory_reservation.Commit();

    return handle_table.SetSize(capabilities.GetHandleTableSize());
}

void Process::Run(s32 main_thread_priority, u64 stack_size) {
    AllocateMainThreadStack(stack_size);
    resource_limit->Reserve(LimitableResource::Threads, 1);
    resource_limit->Reserve(LimitableResource::PhysicalMemory, main_thread_stack_size);

    const std::size_t heap_capacity{memory_usage_capacity - main_thread_stack_size - image_size};
    ASSERT(!page_table->SetHeapCapacity(heap_capacity).IsError());

    ChangeStatus(ProcessStatus::Running);

    SetupMainThread(system, *this, main_thread_priority, main_thread_stack_top);
}

void Process::PrepareForTermination() {
    ChangeStatus(ProcessStatus::Exiting);

    const auto stop_threads = [this](const std::vector<std::shared_ptr<KThread>>& thread_list) {
        for (auto& thread : thread_list) {
            if (thread->GetOwnerProcess() != this)
                continue;

            if (thread.get() == kernel.CurrentScheduler()->GetCurrentThread())
                continue;

            // TODO(Subv): When are the other running/ready threads terminated?
            ASSERT_MSG(thread->GetState() == ThreadState::Waiting,
                       "Exiting processes with non-waiting threads is currently unimplemented");

            thread->Exit();
        }
    };

    stop_threads(system.GlobalSchedulerContext().GetThreadList());

    FreeTLSRegion(tls_region_address);
    tls_region_address = 0;

    if (resource_limit) {
        resource_limit->Release(LimitableResource::PhysicalMemory,
                                main_thread_stack_size + image_size);
    }

    ChangeStatus(ProcessStatus::Exited);
}

/**
 * Attempts to find a TLS page that contains a free slot for
 * use by a thread.
 *
 * @returns If a page with an available slot is found, then an iterator
 *          pointing to the page is returned. Otherwise the end iterator
 *          is returned instead.
 */
static auto FindTLSPageWithAvailableSlots(std::vector<TLSPage>& tls_pages) {
    return std::find_if(tls_pages.begin(), tls_pages.end(),
                        [](const auto& page) { return page.HasAvailableSlots(); });
}

VAddr Process::CreateTLSRegion() {
    KScopedSchedulerLock lock(system.Kernel());
    if (auto tls_page_iter{FindTLSPageWithAvailableSlots(tls_pages)};
        tls_page_iter != tls_pages.cend()) {
        return *tls_page_iter->ReserveSlot();
    }

    Memory::Page* const tls_page_ptr{kernel.GetUserSlabHeapPages().Allocate()};
    ASSERT(tls_page_ptr);

    const VAddr start{page_table->GetKernelMapRegionStart()};
    const VAddr size{page_table->GetKernelMapRegionEnd() - start};
    const PAddr tls_map_addr{system.DeviceMemory().GetPhysicalAddr(tls_page_ptr)};
    const VAddr tls_page_addr{
        page_table
            ->AllocateAndMapMemory(1, Memory::PageSize, true, start, size / Memory::PageSize,
                                   Memory::MemoryState::ThreadLocal,
                                   Memory::MemoryPermission::ReadAndWrite, tls_map_addr)
            .ValueOr(0)};

    ASSERT(tls_page_addr);

    std::memset(tls_page_ptr, 0, Memory::PageSize);
    tls_pages.emplace_back(tls_page_addr);

    const auto reserve_result{tls_pages.back().ReserveSlot()};
    ASSERT(reserve_result.has_value());

    return *reserve_result;
}

void Process::FreeTLSRegion(VAddr tls_address) {
    KScopedSchedulerLock lock(system.Kernel());
    const VAddr aligned_address = Common::AlignDown(tls_address, Core::Memory::PAGE_SIZE);
    auto iter =
        std::find_if(tls_pages.begin(), tls_pages.end(), [aligned_address](const auto& page) {
            return page.GetBaseAddress() == aligned_address;
        });

    // Something has gone very wrong if we're freeing a region
    // with no actual page available.
    ASSERT(iter != tls_pages.cend());

    iter->ReleaseSlot(tls_address);
}

void Process::LoadModule(CodeSet code_set, VAddr base_addr) {
    std::lock_guard lock{HLE::g_hle_lock};
    const auto ReprotectSegment = [&](const CodeSet::Segment& segment,
                                      Memory::MemoryPermission permission) {
        page_table->SetCodeMemoryPermission(segment.addr + base_addr, segment.size, permission);
    };

    system.Memory().WriteBlock(*this, base_addr, code_set.memory.data(), code_set.memory.size());

    ReprotectSegment(code_set.CodeSegment(), Memory::MemoryPermission::ReadAndExecute);
    ReprotectSegment(code_set.RODataSegment(), Memory::MemoryPermission::Read);
    ReprotectSegment(code_set.DataSegment(), Memory::MemoryPermission::ReadAndWrite);
}

bool Process::IsSignaled() const {
    ASSERT(kernel.GlobalSchedulerContext().IsLocked());
    return is_signaled;
}

Process::Process(Core::System& system)
    : KSynchronizationObject{system.Kernel()},
      page_table{std::make_unique<Memory::PageTable>(system)}, handle_table{system.Kernel()},
      address_arbiter{system}, condition_var{system}, state_lock{system.Kernel()}, system{system} {}

Process::~Process() = default;

void Process::ChangeStatus(ProcessStatus new_status) {
    if (status == new_status) {
        return;
    }

    status = new_status;
    is_signaled = true;
    NotifyAvailable();
}

ResultCode Process::AllocateMainThreadStack(std::size_t stack_size) {
    ASSERT(stack_size);

    // The kernel always ensures that the given stack size is page aligned.
    main_thread_stack_size = Common::AlignUp(stack_size, Memory::PageSize);

    const VAddr start{page_table->GetStackRegionStart()};
    const std::size_t size{page_table->GetStackRegionEnd() - start};

    CASCADE_RESULT(main_thread_stack_top,
                   page_table->AllocateAndMapMemory(
                       main_thread_stack_size / Memory::PageSize, Memory::PageSize, false, start,
                       size / Memory::PageSize, Memory::MemoryState::Stack,
                       Memory::MemoryPermission::ReadAndWrite));

    main_thread_stack_top += main_thread_stack_size;

    return RESULT_SUCCESS;
}

} // namespace Kernel
