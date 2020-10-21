// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <atomic>
#include <bitset>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>
#include <utility>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/thread.h"
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
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory/memory_layout.h"
#include "core/hle/kernel/memory/memory_manager.h"
#include "core/hle/kernel/memory/slab_heap.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/kernel/synchronization.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/time_manager.h"
#include "core/hle/lock.h"
#include "core/hle/result.h"
#include "core/memory.h"

MICROPROFILE_DEFINE(Kernel_SVC, "Kernel", "SVC", MP_RGB(70, 200, 70));

namespace Kernel {

struct KernelCore::Impl {
    explicit Impl(Core::System& system, KernelCore& kernel)
        : global_scheduler{kernel}, synchronization{system}, time_manager{system},
          global_handle_table{kernel}, system{system} {}

    void SetMulticore(bool is_multicore) {
        this->is_multicore = is_multicore;
    }

    void Initialize(KernelCore& kernel) {
        Shutdown();
        RegisterHostThread();

        InitializePhysicalCores();
        InitializeSystemResourceLimit(kernel);
        InitializeMemoryLayout();
        InitializePreemption(kernel);
        InitializeSchedulers();
        InitializeSuspendThreads();
    }

    void Shutdown() {
        next_object_id = 0;
        next_kernel_process_id = Process::InitialKIPIDMin;
        next_user_process_id = Process::ProcessIDMin;
        next_thread_id = 1;

        for (std::size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            if (suspend_threads[i]) {
                suspend_threads[i].reset();
            }
        }

        for (std::size_t i = 0; i < cores.size(); i++) {
            cores[i].Shutdown();
            schedulers[i].reset();
        }
        cores.clear();

        registered_core_threads.reset();

        process_list.clear();
        current_process = nullptr;

        system_resource_limit = nullptr;

        global_handle_table.Clear();
        preemption_event = nullptr;

        global_scheduler.Shutdown();

        named_ports.clear();

        for (auto& core : cores) {
            core.Shutdown();
        }
        cores.clear();

        exclusive_monitor.reset();

        num_host_threads = 0;
        std::fill(register_host_thread_keys.begin(), register_host_thread_keys.end(),
                  std::thread::id{});
        std::fill(register_host_thread_values.begin(), register_host_thread_values.end(), 0);
    }

    void InitializePhysicalCores() {
        exclusive_monitor =
            Core::MakeExclusiveMonitor(system.Memory(), Core::Hardware::NUM_CPU_CORES);
        for (std::size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            schedulers[i] = std::make_unique<Kernel::Scheduler>(system, i);
            cores.emplace_back(system, i, *schedulers[i], interrupts[i]);
        }
    }

    void InitializeSchedulers() {
        for (std::size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            cores[i].Scheduler().Initialize();
        }
    }

    // Creates the default system resource limit
    void InitializeSystemResourceLimit(KernelCore& kernel) {
        system_resource_limit = ResourceLimit::Create(kernel);

        // If setting the default system values fails, then something seriously wrong has occurred.
        ASSERT(system_resource_limit->SetLimitValue(ResourceType::PhysicalMemory, 0x100000000)
                   .IsSuccess());
        ASSERT(system_resource_limit->SetLimitValue(ResourceType::Threads, 800).IsSuccess());
        ASSERT(system_resource_limit->SetLimitValue(ResourceType::Events, 700).IsSuccess());
        ASSERT(system_resource_limit->SetLimitValue(ResourceType::TransferMemory, 200).IsSuccess());
        ASSERT(system_resource_limit->SetLimitValue(ResourceType::Sessions, 900).IsSuccess());

        if (!system_resource_limit->Reserve(ResourceType::PhysicalMemory, 0) ||
            !system_resource_limit->Reserve(ResourceType::PhysicalMemory, 0x60000)) {
            UNREACHABLE();
        }
    }

    void InitializePreemption(KernelCore& kernel) {
        preemption_event = Core::Timing::CreateEvent(
            "PreemptionCallback", [this, &kernel](std::uintptr_t, std::chrono::nanoseconds) {
                {
                    SchedulerLock lock(kernel);
                    global_scheduler.PreemptThreads();
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
            const auto type =
                static_cast<ThreadType>(THREADTYPE_KERNEL | THREADTYPE_HLE | THREADTYPE_SUSPEND);
            auto thread_res =
                Thread::Create(system, type, std::move(name), 0, 0, 0, static_cast<u32>(i), 0,
                               nullptr, std::move(init_func), init_func_parameter);

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

    void RegisterCoreThread(std::size_t core_id) {
        const std::thread::id this_id = std::this_thread::get_id();
        if (!is_multicore) {
            single_core_thread_id = this_id;
        }
        const auto end =
            register_host_thread_keys.begin() + static_cast<ptrdiff_t>(num_host_threads);
        const auto it = std::find(register_host_thread_keys.begin(), end, this_id);
        ASSERT(core_id < Core::Hardware::NUM_CPU_CORES);
        ASSERT(it == end);
        ASSERT(!registered_core_threads[core_id]);
        InsertHostThread(static_cast<u32>(core_id));
        registered_core_threads.set(core_id);
    }

    void RegisterHostThread() {
        const std::thread::id this_id = std::this_thread::get_id();
        const auto end =
            register_host_thread_keys.begin() + static_cast<ptrdiff_t>(num_host_threads);
        const auto it = std::find(register_host_thread_keys.begin(), end, this_id);
        if (it == end) {
            InsertHostThread(registered_thread_ids++);
        }
    }

    void InsertHostThread(u32 value) {
        const size_t index = num_host_threads++;
        ASSERT_MSG(index < NUM_REGISTRABLE_HOST_THREADS, "Too many host threads");
        register_host_thread_values[index] = value;
        register_host_thread_keys[index] = std::this_thread::get_id();
    }

    [[nodiscard]] u32 GetCurrentHostThreadID() const {
        const std::thread::id this_id = std::this_thread::get_id();
        if (!is_multicore && single_core_thread_id == this_id) {
            return static_cast<u32>(system.GetCpuManager().CurrentCore());
        }
        const auto end =
            register_host_thread_keys.begin() + static_cast<ptrdiff_t>(num_host_threads);
        const auto it = std::find(register_host_thread_keys.begin(), end, this_id);
        if (it == end) {
            return Core::INVALID_HOST_THREAD_ID;
        }
        return register_host_thread_values[static_cast<size_t>(
            std::distance(register_host_thread_keys.begin(), it))];
    }

    Core::EmuThreadHandle GetCurrentEmuThreadID() const {
        Core::EmuThreadHandle result = Core::EmuThreadHandle::InvalidHandle();
        result.host_handle = GetCurrentHostThreadID();
        if (result.host_handle >= Core::Hardware::NUM_CPU_CORES) {
            return result;
        }
        const Kernel::Scheduler& sched = cores[result.host_handle].Scheduler();
        const Kernel::Thread* current = sched.GetCurrentThread();
        if (current != nullptr && !current->IsPhantomMode()) {
            result.guest_handle = current->GetGlobalHandle();
        } else {
            result.guest_handle = InvalidHandle;
        }
        return result;
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

        // Initialize slab heaps
        constexpr u64 user_slab_heap_size{0x3de000};
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
    Kernel::GlobalScheduler global_scheduler;
    Kernel::Synchronization synchronization;
    Kernel::TimeManager time_manager;

    std::shared_ptr<ResourceLimit> system_resource_limit;

    std::shared_ptr<Core::Timing::EventType> preemption_event;

    // This is the kernel's handle table or supervisor handle table which
    // stores all the objects in place.
    HandleTable global_handle_table;

    /// Map of named ports managed by the kernel, which can be retrieved using
    /// the ConnectToPort SVC.
    NamedPortTable named_ports;

    std::unique_ptr<Core::ExclusiveMonitor> exclusive_monitor;
    std::vector<Kernel::PhysicalCore> cores;

    // 0-3 IDs represent core threads, >3 represent others
    std::atomic<u32> registered_thread_ids{Core::Hardware::NUM_CPU_CORES};
    std::bitset<Core::Hardware::NUM_CPU_CORES> registered_core_threads;

    // Number of host threads is a relatively high number to avoid overflowing
    static constexpr size_t NUM_REGISTRABLE_HOST_THREADS = 64;
    std::atomic<size_t> num_host_threads{0};
    std::array<std::atomic<std::thread::id>, NUM_REGISTRABLE_HOST_THREADS>
        register_host_thread_keys{};
    std::array<std::atomic<u32>, NUM_REGISTRABLE_HOST_THREADS> register_host_thread_values{};

    // Kernel memory management
    std::unique_ptr<Memory::MemoryManager> memory_manager;
    std::unique_ptr<Memory::SlabHeap<Memory::Page>> user_slab_heap_pages;

    // Shared memory for services
    std::shared_ptr<Kernel::SharedMemory> hid_shared_mem;
    std::shared_ptr<Kernel::SharedMemory> font_shared_mem;
    std::shared_ptr<Kernel::SharedMemory> irs_shared_mem;
    std::shared_ptr<Kernel::SharedMemory> time_shared_mem;

    std::array<std::shared_ptr<Thread>, Core::Hardware::NUM_CPU_CORES> suspend_threads{};
    std::array<Core::CPUInterruptHandler, Core::Hardware::NUM_CPU_CORES> interrupts{};
    std::array<std::unique_ptr<Kernel::Scheduler>, Core::Hardware::NUM_CPU_CORES> schedulers{};

    bool is_multicore{};
    std::thread::id single_core_thread_id{};

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

void KernelCore::Shutdown() {
    impl->Shutdown();
}

std::shared_ptr<ResourceLimit> KernelCore::GetSystemResourceLimit() const {
    return impl->system_resource_limit;
}

std::shared_ptr<Thread> KernelCore::RetrieveThreadFromGlobalHandleTable(Handle handle) const {
    return impl->global_handle_table.Get<Thread>(handle);
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

Kernel::GlobalScheduler& KernelCore::GlobalScheduler() {
    return impl->global_scheduler;
}

const Kernel::GlobalScheduler& KernelCore::GlobalScheduler() const {
    return impl->global_scheduler;
}

Kernel::Scheduler& KernelCore::Scheduler(std::size_t id) {
    return *impl->schedulers[id];
}

const Kernel::Scheduler& KernelCore::Scheduler(std::size_t id) const {
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

Kernel::Scheduler& KernelCore::CurrentScheduler() {
    u32 core_id = impl->GetCurrentHostThreadID();
    ASSERT(core_id < Core::Hardware::NUM_CPU_CORES);
    return *impl->schedulers[core_id];
}

const Kernel::Scheduler& KernelCore::CurrentScheduler() const {
    u32 core_id = impl->GetCurrentHostThreadID();
    ASSERT(core_id < Core::Hardware::NUM_CPU_CORES);
    return *impl->schedulers[core_id];
}

std::array<Core::CPUInterruptHandler, Core::Hardware::NUM_CPU_CORES>& KernelCore::Interrupts() {
    return impl->interrupts;
}

const std::array<Core::CPUInterruptHandler, Core::Hardware::NUM_CPU_CORES>& KernelCore::Interrupts()
    const {
    return impl->interrupts;
}

Kernel::Synchronization& KernelCore::Synchronization() {
    return impl->synchronization;
}

const Kernel::Synchronization& KernelCore::Synchronization() const {
    return impl->synchronization;
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
    auto& threads = GlobalScheduler().GetThreadList();
    for (auto& thread : threads) {
        if (!thread->IsHLEThread()) {
            auto& arm_interface = thread->ArmInterface();
            arm_interface.ClearInstructionCache();
        }
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

Core::EmuThreadHandle KernelCore::GetCurrentEmuThreadID() const {
    return impl->GetCurrentEmuThreadID();
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
        SchedulerLock lock(*this);
        ThreadStatus status = should_suspend ? ThreadStatus::Ready : ThreadStatus::WaitSleep;
        for (std::size_t i = 0; i < Core::Hardware::NUM_CPU_CORES; i++) {
            impl->suspend_threads[i]->SetStatus(status);
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

} // namespace Kernel
