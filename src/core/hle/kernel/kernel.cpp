// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <atomic>
#include <bitset>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_set>
#include <utility>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/thread.h"
#include "common/thread_worker.h"
#include "core/arm/arm_interface.h"
#include "core/arm/cpu_interrupt_handler.h"
#include "core/arm/exclusive_monitor.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/cpu_manager.h"
#include "core/device_memory.h"
#include "core/hardware_properties.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/k_resource_limit.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory/memory_layout.h"
#include "core/hle/kernel/memory/memory_manager.h"
#include "core/hle/kernel/memory/slab_heap.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/service_thread.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/kernel/time_manager.h"
#include "core/hle/lock.h"
#include "core/hle/result.h"
#include "core/memory.h"

MICROPROFILE_DEFINE(Kernel_SVC, "Kernel", "SVC", MP_RGB(70, 200, 70));

namespace Kernel {

struct KernelCore::Impl {
    explicit Impl(Core::System& system, KernelCore& kernel)
        : time_manager{system}, global_handle_table{kernel}, system{system} {}

    void SetMulticore(bool is_multicore) {
        this->is_multicore = is_multicore;
    }

    void Initialize(KernelCore& kernel) {
        global_scheduler_context = std::make_unique<Kernel::GlobalSchedulerContext>(kernel);

        RegisterHostThread();

        service_thread_manager =
            std::make_unique<Common::ThreadWorker>(1, "yuzu:ServiceThreadManager");
        is_phantom_mode_for_singlecore = false;

        InitializePhysicalCores();
        InitializeSystemResourceLimit(kernel, system);
        InitializeMemoryLayout();
        InitializePreemption(kernel);
        InitializeSchedulers();
        InitializeSuspendThreads();
    }

    void InitializeCores() {
        for (auto& core : cores) {
            core.Initialize(current_process->Is64BitProcess());
        }
    }

    void Shutdown() {
        process_list.clear();

        // Ensures all service threads gracefully shutdown
        service_thread_manager.reset();
        service_threads.clear();

        next_object_id = 0;
        next_kernel_process_id = Process::InitialKIPIDMin;
        next_user_process_id = Process::ProcessIDMin;
        next_thread_id = 1;

        for (std::size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            if (suspend_threads[i]) {
                suspend_threads[i].reset();
            }
        }

        cores.clear();

        current_process = nullptr;

        system_resource_limit = nullptr;

        global_handle_table.Clear();

        preemption_event = nullptr;

        named_ports.clear();

        exclusive_monitor.reset();

        // Next host thead ID to use, 0-3 IDs represent core threads, >3 represent others
        next_host_thread_id = Core::Hardware::NUM_CPU_CORES;
    }

    void InitializePhysicalCores() {
        exclusive_monitor =
            Core::MakeExclusiveMonitor(system.Memory(), Core::Hardware::NUM_CPU_CORES);
        for (u32 i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            schedulers[i] = std::make_unique<Kernel::KScheduler>(system, i);
            cores.emplace_back(i, system, *schedulers[i], interrupts);
        }
    }

    void InitializeSchedulers() {
        for (u32 i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            cores[i].Scheduler().Initialize();
        }
    }

    // Creates the default system resource limit
    void InitializeSystemResourceLimit(KernelCore& kernel, Core::System& system) {
        system_resource_limit = std::make_shared<KResourceLimit>(kernel, system);

        // If setting the default system values fails, then something seriously wrong has occurred.
        ASSERT(system_resource_limit->SetLimitValue(LimitableResource::PhysicalMemory, 0x100000000)
                   .IsSuccess());
        ASSERT(system_resource_limit->SetLimitValue(LimitableResource::Threads, 800).IsSuccess());
        ASSERT(system_resource_limit->SetLimitValue(LimitableResource::Events, 700).IsSuccess());
        ASSERT(system_resource_limit->SetLimitValue(LimitableResource::TransferMemory, 200)
                   .IsSuccess());
        ASSERT(system_resource_limit->SetLimitValue(LimitableResource::Sessions, 933).IsSuccess());

        // Derived from recent software updates. The kernel reserves 27MB
        constexpr u64 kernel_size{0x1b00000};
        if (!system_resource_limit->Reserve(LimitableResource::PhysicalMemory, kernel_size)) {
            UNREACHABLE();
        }
        // Reserve secure applet memory, introduced in firmware 5.0.0
        constexpr u64 secure_applet_memory_size{0x400000};
        ASSERT(system_resource_limit->Reserve(LimitableResource::PhysicalMemory,
                                              secure_applet_memory_size));
    }

    void InitializePreemption(KernelCore& kernel) {
        preemption_event = Core::Timing::CreateEvent(
            "PreemptionCallback", [this, &kernel](std::uintptr_t, std::chrono::nanoseconds) {
                {
                    KScopedSchedulerLock lock(kernel);
                    global_scheduler_context->PreemptThreads();
                }
                const auto time_interval = std::chrono::nanoseconds{
                    Core::Timing::msToCycles(std::chrono::milliseconds(10))};
                system.CoreTiming().ScheduleEvent(time_interval, preemption_event);
            });

        const auto time_interval =
            std::chrono::nanoseconds{Core::Timing::msToCycles(std::chrono::milliseconds(10))};
        system.CoreTiming().ScheduleEvent(time_interval, preemption_event);
    }

    void InitializeSuspendThreads() {
        for (std::size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            std::string name = "Suspend Thread Id:" + std::to_string(i);
            std::function<void(void*)> init_func = Core::CpuManager::GetSuspendThreadStartFunc();
            void* init_func_parameter = system.GetCpuManager().GetStartFuncParamater();
            auto thread_res = KThread::Create(system, ThreadType::HighPriority, std::move(name), 0,
                                              0, 0, static_cast<u32>(i), 0, nullptr,
                                              std::move(init_func), init_func_parameter);

            suspend_threads[i] = std::move(thread_res).Unwrap();
        }
    }

    void MakeCurrentProcess(Process* process) {
        current_process = process;
        if (process == nullptr) {
            return;
        }

        const u32 core_id = GetCurrentHostThreadID();
        if (core_id < Core::Hardware::NUM_CPU_CORES) {
            system.Memory().SetCurrentPageTable(*process, core_id);
        }
    }

    /// Creates a new host thread ID, should only be called by GetHostThreadId
    u32 AllocateHostThreadId(std::optional<std::size_t> core_id) {
        if (core_id) {
            // The first for slots are reserved for CPU core threads
            ASSERT(*core_id < Core::Hardware::NUM_CPU_CORES);
            return static_cast<u32>(*core_id);
        } else {
            return next_host_thread_id++;
        }
    }

    /// Gets the host thread ID for the caller, allocating a new one if this is the first time
    u32 GetHostThreadId(std::optional<std::size_t> core_id = std::nullopt) {
        const thread_local auto host_thread_id{AllocateHostThreadId(core_id)};
        return host_thread_id;
    }

    // Gets the dummy KThread for the caller, allocating a new one if this is the first time
    KThread* GetHostDummyThread() {
        const thread_local auto thread =
            KThread::Create(
                system, ThreadType::Main, fmt::format("DummyThread:{}", GetHostThreadId()), 0,
                KThread::DefaultThreadPriority, 0, static_cast<u32>(3), 0, nullptr,
                []([[maybe_unused]] void* arg) { UNREACHABLE(); }, nullptr)
                .Unwrap();
        return thread.get();
    }

    /// Registers a CPU core thread by allocating a host thread ID for it
    void RegisterCoreThread(std::size_t core_id) {
        ASSERT(core_id < Core::Hardware::NUM_CPU_CORES);
        const auto this_id = GetHostThreadId(core_id);
        if (!is_multicore) {
            single_core_thread_id = this_id;
        }
    }

    /// Registers a new host thread by allocating a host thread ID for it
    void RegisterHostThread() {
        [[maybe_unused]] const auto this_id = GetHostThreadId();
        [[maybe_unused]] const auto dummy_thread = GetHostDummyThread();
    }

    [[nodiscard]] u32 GetCurrentHostThreadID() {
        const auto this_id = GetHostThreadId();
        if (!is_multicore && single_core_thread_id == this_id) {
            return static_cast<u32>(system.GetCpuManager().CurrentCore());
        }
        return this_id;
    }

    bool IsPhantomModeForSingleCore() const {
        return is_phantom_mode_for_singlecore;
    }

    void SetIsPhantomModeForSingleCore(bool value) {
        ASSERT(!is_multicore);
        is_phantom_mode_for_singlecore = value;
    }

    KThread* GetCurrentEmuThread() {
        const auto thread_id = GetCurrentHostThreadID();
        if (thread_id >= Core::Hardware::NUM_CPU_CORES) {
            return GetHostDummyThread();
        }
        return schedulers[thread_id]->GetCurrentThread();
    }

    void InitializeMemoryLayout() {
        // Initialize memory layout
        constexpr Memory::MemoryLayout layout{Memory::MemoryLayout::GetDefaultLayout()};
        constexpr std::size_t hid_size{0x40000};
        constexpr std::size_t font_size{0x1100000};
        constexpr std::size_t irs_size{0x8000};
        constexpr std::size_t time_size{0x1000};
        constexpr PAddr hid_addr{layout.System().StartAddress()};
        constexpr PAddr font_pa{layout.System().StartAddress() + hid_size};
        constexpr PAddr irs_addr{layout.System().StartAddress() + hid_size + font_size};
        constexpr PAddr time_addr{layout.System().StartAddress() + hid_size + font_size + irs_size};

        // Initialize memory manager
        memory_manager = std::make_unique<Memory::MemoryManager>();
        memory_manager->InitializeManager(Memory::MemoryManager::Pool::Application,
                                          layout.Application().StartAddress(),
                                          layout.Application().EndAddress());
        memory_manager->InitializeManager(Memory::MemoryManager::Pool::Applet,
                                          layout.Applet().StartAddress(),
                                          layout.Applet().EndAddress());
        memory_manager->InitializeManager(Memory::MemoryManager::Pool::System,
                                          layout.System().StartAddress(),
                                          layout.System().EndAddress());

        hid_shared_mem = Kernel::SharedMemory::Create(
            system.Kernel(), system.DeviceMemory(), nullptr,
            {hid_addr, hid_size / Memory::PageSize}, Memory::MemoryPermission::None,
            Memory::MemoryPermission::Read, hid_addr, hid_size, "HID:SharedMemory");
        font_shared_mem = Kernel::SharedMemory::Create(
            system.Kernel(), system.DeviceMemory(), nullptr,
            {font_pa, font_size / Memory::PageSize}, Memory::MemoryPermission::None,
            Memory::MemoryPermission::Read, font_pa, font_size, "Font:SharedMemory");
        irs_shared_mem = Kernel::SharedMemory::Create(
            system.Kernel(), system.DeviceMemory(), nullptr,
            {irs_addr, irs_size / Memory::PageSize}, Memory::MemoryPermission::None,
            Memory::MemoryPermission::Read, irs_addr, irs_size, "IRS:SharedMemory");
        time_shared_mem = Kernel::SharedMemory::Create(
            system.Kernel(), system.DeviceMemory(), nullptr,
            {time_addr, time_size / Memory::PageSize}, Memory::MemoryPermission::None,
            Memory::MemoryPermission::Read, time_addr, time_size, "Time:SharedMemory");

        // Allocate slab heaps
        user_slab_heap_pages = std::make_unique<Memory::SlabHeap<Memory::Page>>();

        constexpr u64 user_slab_heap_size{0x1ef000};
        // Reserve slab heaps
        ASSERT(
            system_resource_limit->Reserve(LimitableResource::PhysicalMemory, user_slab_heap_size));
        // Initialize slab heaps
        user_slab_heap_pages->Initialize(
            system.DeviceMemory().GetPointer(Core::DramMemoryMap::SlabHeapBase),
            user_slab_heap_size);
    }

    std::atomic<u32> next_object_id{0};
    std::atomic<u64> next_kernel_process_id{Process::InitialKIPIDMin};
    std::atomic<u64> next_user_process_id{Process::ProcessIDMin};
    std::atomic<u64> next_thread_id{1};

    // Lists all processes that exist in the current session.
    std::vector<std::shared_ptr<Process>> process_list;
    Process* current_process = nullptr;
    std::unique_ptr<Kernel::GlobalSchedulerContext> global_scheduler_context;
    Kernel::TimeManager time_manager;

    std::shared_ptr<KResourceLimit> system_resource_limit;

    std::shared_ptr<Core::Timing::EventType> preemption_event;

    // This is the kernel's handle table or supervisor handle table which
    // stores all the objects in place.
    HandleTable global_handle_table;

    /// Map of named ports managed by the kernel, which can be retrieved using
    /// the ConnectToPort SVC.
    NamedPortTable named_ports;

    std::unique_ptr<Core::ExclusiveMonitor> exclusive_monitor;
    std::vector<Kernel::PhysicalCore> cores;

    // Next host thead ID to use, 0-3 IDs represent core threads, >3 represent others
    std::atomic<u32> next_host_thread_id{Core::Hardware::NUM_CPU_CORES};

    // Kernel memory management
    std::unique_ptr<Memory::MemoryManager> memory_manager;
    std::unique_ptr<Memory::SlabHeap<Memory::Page>> user_slab_heap_pages;

    // Shared memory for services
    std::shared_ptr<Kernel::SharedMemory> hid_shared_mem;
    std::shared_ptr<Kernel::SharedMemory> font_shared_mem;
    std::shared_ptr<Kernel::SharedMemory> irs_shared_mem;
    std::shared_ptr<Kernel::SharedMemory> time_shared_mem;

    // Threads used for services
    std::unordered_set<std::shared_ptr<Kernel::ServiceThread>> service_threads;

    // Service threads are managed by a worker thread, so that a calling service thread can queue up
    // the release of itself
    std::unique_ptr<Common::ThreadWorker> service_thread_manager;

    std::array<std::shared_ptr<KThread>, Core::Hardware::NUM_CPU_CORES> suspend_threads{};
    std::array<Core::CPUInterruptHandler, Core::Hardware::NUM_CPU_CORES> interrupts{};
    std::array<std::unique_ptr<Kernel::KScheduler>, Core::Hardware::NUM_CPU_CORES> schedulers{};

    bool is_multicore{};
    bool is_phantom_mode_for_singlecore{};
    u32 single_core_thread_id{};

    std::array<u64, Core::Hardware::NUM_CPU_CORES> svc_ticks{};

    // System context
    Core::System& system;
};

KernelCore::KernelCore(Core::System& system) : impl{std::make_unique<Impl>(system, *this)} {}
KernelCore::~KernelCore() {
    Shutdown();
}

void KernelCore::SetMulticore(bool is_multicore) {
    impl->SetMulticore(is_multicore);
}

void KernelCore::Initialize() {
    impl->Initialize(*this);
}

void KernelCore::InitializeCores() {
    impl->InitializeCores();
}

void KernelCore::Shutdown() {
    impl->Shutdown();
}

std::shared_ptr<KResourceLimit> KernelCore::GetSystemResourceLimit() const {
    return impl->system_resource_limit;
}

std::shared_ptr<KThread> KernelCore::RetrieveThreadFromGlobalHandleTable(Handle handle) const {
    return impl->global_handle_table.Get<KThread>(handle);
}

void KernelCore::AppendNewProcess(std::shared_ptr<Process> process) {
    impl->process_list.push_back(std::move(process));
}

void KernelCore::MakeCurrentProcess(Process* process) {
    impl->MakeCurrentProcess(process);
}

Process* KernelCore::CurrentProcess() {
    return impl->current_process;
}

const Process* KernelCore::CurrentProcess() const {
    return impl->current_process;
}

const std::vector<std::shared_ptr<Process>>& KernelCore::GetProcessList() const {
    return impl->process_list;
}

Kernel::GlobalSchedulerContext& KernelCore::GlobalSchedulerContext() {
    return *impl->global_scheduler_context;
}

const Kernel::GlobalSchedulerContext& KernelCore::GlobalSchedulerContext() const {
    return *impl->global_scheduler_context;
}

Kernel::KScheduler& KernelCore::Scheduler(std::size_t id) {
    return *impl->schedulers[id];
}

const Kernel::KScheduler& KernelCore::Scheduler(std::size_t id) const {
    return *impl->schedulers[id];
}

Kernel::PhysicalCore& KernelCore::PhysicalCore(std::size_t id) {
    return impl->cores[id];
}

const Kernel::PhysicalCore& KernelCore::PhysicalCore(std::size_t id) const {
    return impl->cores[id];
}

Kernel::PhysicalCore& KernelCore::CurrentPhysicalCore() {
    u32 core_id = impl->GetCurrentHostThreadID();
    ASSERT(core_id < Core::Hardware::NUM_CPU_CORES);
    return impl->cores[core_id];
}

const Kernel::PhysicalCore& KernelCore::CurrentPhysicalCore() const {
    u32 core_id = impl->GetCurrentHostThreadID();
    ASSERT(core_id < Core::Hardware::NUM_CPU_CORES);
    return impl->cores[core_id];
}

Kernel::KScheduler* KernelCore::CurrentScheduler() {
    u32 core_id = impl->GetCurrentHostThreadID();
    if (core_id >= Core::Hardware::NUM_CPU_CORES) {
        // This is expected when called from not a guest thread
        return {};
    }
    return impl->schedulers[core_id].get();
}

std::array<Core::CPUInterruptHandler, Core::Hardware::NUM_CPU_CORES>& KernelCore::Interrupts() {
    return impl->interrupts;
}

const std::array<Core::CPUInterruptHandler, Core::Hardware::NUM_CPU_CORES>& KernelCore::Interrupts()
    const {
    return impl->interrupts;
}

Kernel::TimeManager& KernelCore::TimeManager() {
    return impl->time_manager;
}

const Kernel::TimeManager& KernelCore::TimeManager() const {
    return impl->time_manager;
}

Core::ExclusiveMonitor& KernelCore::GetExclusiveMonitor() {
    return *impl->exclusive_monitor;
}

const Core::ExclusiveMonitor& KernelCore::GetExclusiveMonitor() const {
    return *impl->exclusive_monitor;
}

void KernelCore::InvalidateAllInstructionCaches() {
    for (auto& physical_core : impl->cores) {
        physical_core.ArmInterface().ClearInstructionCache();
    }
}

void KernelCore::InvalidateCpuInstructionCacheRange(VAddr addr, std::size_t size) {
    for (auto& physical_core : impl->cores) {
        if (!physical_core.IsInitialized()) {
            continue;
        }
        physical_core.ArmInterface().InvalidateCacheRange(addr, size);
    }
}

void KernelCore::PrepareReschedule(std::size_t id) {
    // TODO: Reimplement, this
}

void KernelCore::AddNamedPort(std::string name, std::shared_ptr<ClientPort> port) {
    impl->named_ports.emplace(std::move(name), std::move(port));
}

KernelCore::NamedPortTable::iterator KernelCore::FindNamedPort(const std::string& name) {
    return impl->named_ports.find(name);
}

KernelCore::NamedPortTable::const_iterator KernelCore::FindNamedPort(
    const std::string& name) const {
    return impl->named_ports.find(name);
}

bool KernelCore::IsValidNamedPort(NamedPortTable::const_iterator port) const {
    return port != impl->named_ports.cend();
}

u32 KernelCore::CreateNewObjectID() {
    return impl->next_object_id++;
}

u64 KernelCore::CreateNewThreadID() {
    return impl->next_thread_id++;
}

u64 KernelCore::CreateNewKernelProcessID() {
    return impl->next_kernel_process_id++;
}

u64 KernelCore::CreateNewUserProcessID() {
    return impl->next_user_process_id++;
}

Kernel::HandleTable& KernelCore::GlobalHandleTable() {
    return impl->global_handle_table;
}

const Kernel::HandleTable& KernelCore::GlobalHandleTable() const {
    return impl->global_handle_table;
}

void KernelCore::RegisterCoreThread(std::size_t core_id) {
    impl->RegisterCoreThread(core_id);
}

void KernelCore::RegisterHostThread() {
    impl->RegisterHostThread();
}

u32 KernelCore::GetCurrentHostThreadID() const {
    return impl->GetCurrentHostThreadID();
}

KThread* KernelCore::GetCurrentEmuThread() const {
    return impl->GetCurrentEmuThread();
}

Memory::MemoryManager& KernelCore::MemoryManager() {
    return *impl->memory_manager;
}

const Memory::MemoryManager& KernelCore::MemoryManager() const {
    return *impl->memory_manager;
}

Memory::SlabHeap<Memory::Page>& KernelCore::GetUserSlabHeapPages() {
    return *impl->user_slab_heap_pages;
}

const Memory::SlabHeap<Memory::Page>& KernelCore::GetUserSlabHeapPages() const {
    return *impl->user_slab_heap_pages;
}

Kernel::SharedMemory& KernelCore::GetHidSharedMem() {
    return *impl->hid_shared_mem;
}

const Kernel::SharedMemory& KernelCore::GetHidSharedMem() const {
    return *impl->hid_shared_mem;
}

Kernel::SharedMemory& KernelCore::GetFontSharedMem() {
    return *impl->font_shared_mem;
}

const Kernel::SharedMemory& KernelCore::GetFontSharedMem() const {
    return *impl->font_shared_mem;
}

Kernel::SharedMemory& KernelCore::GetIrsSharedMem() {
    return *impl->irs_shared_mem;
}

const Kernel::SharedMemory& KernelCore::GetIrsSharedMem() const {
    return *impl->irs_shared_mem;
}

Kernel::SharedMemory& KernelCore::GetTimeSharedMem() {
    return *impl->time_shared_mem;
}

const Kernel::SharedMemory& KernelCore::GetTimeSharedMem() const {
    return *impl->time_shared_mem;
}

void KernelCore::Suspend(bool in_suspention) {
    const bool should_suspend = exception_exited || in_suspention;
    {
        KScopedSchedulerLock lock(*this);
        const auto state = should_suspend ? ThreadState::Runnable : ThreadState::Waiting;
        for (std::size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            impl->suspend_threads[i]->SetState(state);
            impl->suspend_threads[i]->SetWaitReasonForDebugging(
                ThreadWaitReasonForDebugging::Suspended);
        }
    }
}

bool KernelCore::IsMulticore() const {
    return impl->is_multicore;
}

void KernelCore::ExceptionalExit() {
    exception_exited = true;
    Suspend(true);
}

void KernelCore::EnterSVCProfile() {
    std::size_t core = impl->GetCurrentHostThreadID();
    impl->svc_ticks[core] = MicroProfileEnter(MICROPROFILE_TOKEN(Kernel_SVC));
}

void KernelCore::ExitSVCProfile() {
    std::size_t core = impl->GetCurrentHostThreadID();
    MicroProfileLeave(MICROPROFILE_TOKEN(Kernel_SVC), impl->svc_ticks[core]);
}

std::weak_ptr<Kernel::ServiceThread> KernelCore::CreateServiceThread(const std::string& name) {
    auto service_thread = std::make_shared<Kernel::ServiceThread>(*this, 1, name);
    impl->service_thread_manager->QueueWork(
        [this, service_thread] { impl->service_threads.emplace(service_thread); });
    return service_thread;
}

void KernelCore::ReleaseServiceThread(std::weak_ptr<Kernel::ServiceThread> service_thread) {
    impl->service_thread_manager->QueueWork([this, service_thread] {
        if (auto strong_ptr = service_thread.lock()) {
            impl->service_threads.erase(strong_ptr);
        }
    });
}

bool KernelCore::IsPhantomModeForSingleCore() const {
    return impl->IsPhantomModeForSingleCore();
}

void KernelCore::SetIsPhantomModeForSingleCore(bool value) {
    impl->SetIsPhantomModeForSingleCore(value);
}

} // namespace Kernel
