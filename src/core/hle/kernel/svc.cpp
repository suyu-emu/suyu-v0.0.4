// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <iterator>
#include <mutex>
#include <vector>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/fiber.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/string_util.h"
#include "core/arm/exclusive_monitor.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/cpu_manager.h"
#include "core/hle/kernel/address_arbiter.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory/memory_block.h"
#include "core/hle/kernel/memory/page_table.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/readable_event.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_types.h"
#include "core/hle/kernel/svc_wrap.h"
#include "core/hle/kernel/synchronization.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/time_manager.h"
#include "core/hle/kernel/transfer_memory.h"
#include "core/hle/kernel/writable_event.h"
#include "core/hle/lock.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"
#include "core/memory.h"
#include "core/reporter.h"

namespace Kernel::Svc {
namespace {

// Checks if address + size is greater than the given address
// This can return false if the size causes an overflow of a 64-bit type
// or if the given size is zero.
constexpr bool IsValidAddressRange(VAddr address, u64 size) {
    return address + size > address;
}

// Helper function that performs the common sanity checks for svcMapMemory
// and svcUnmapMemory. This is doable, as both functions perform their sanitizing
// in the same order.
ResultCode MapUnmapMemorySanityChecks(const Memory::PageTable& manager, VAddr dst_addr,
                                      VAddr src_addr, u64 size) {
    if (!Common::Is4KBAligned(dst_addr)) {
        LOG_ERROR(Kernel_SVC, "Destination address is not aligned to 4KB, 0x{:016X}", dst_addr);
        return ERR_INVALID_ADDRESS;
    }

    if (!Common::Is4KBAligned(src_addr)) {
        LOG_ERROR(Kernel_SVC, "Source address is not aligned to 4KB, 0x{:016X}", src_addr);
        return ERR_INVALID_SIZE;
    }

    if (size == 0) {
        LOG_ERROR(Kernel_SVC, "Size is 0");
        return ERR_INVALID_SIZE;
    }

    if (!Common::Is4KBAligned(size)) {
        LOG_ERROR(Kernel_SVC, "Size is not aligned to 4KB, 0x{:016X}", size);
        return ERR_INVALID_SIZE;
    }

    if (!IsValidAddressRange(dst_addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Destination is not a valid address range, addr=0x{:016X}, size=0x{:016X}",
                  dst_addr, size);
        return ERR_INVALID_ADDRESS_STATE;
    }

    if (!IsValidAddressRange(src_addr, size)) {
        LOG_ERROR(Kernel_SVC, "Source is not a valid address range, addr=0x{:016X}, size=0x{:016X}",
                  src_addr, size);
        return ERR_INVALID_ADDRESS_STATE;
    }

    if (!manager.IsInsideAddressSpace(src_addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Source is not within the address space, addr=0x{:016X}, size=0x{:016X}",
                  src_addr, size);
        return ERR_INVALID_ADDRESS_STATE;
    }

    if (manager.IsOutsideStackRegion(dst_addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Destination is not within the stack region, addr=0x{:016X}, size=0x{:016X}",
                  dst_addr, size);
        return ERR_INVALID_MEMORY_RANGE;
    }

    if (manager.IsInsideHeapRegion(dst_addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Destination does not fit within the heap region, addr=0x{:016X}, "
                  "size=0x{:016X}",
                  dst_addr, size);
        return ERR_INVALID_MEMORY_RANGE;
    }

    if (manager.IsInsideAliasRegion(dst_addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Destination does not fit within the map region, addr=0x{:016X}, "
                  "size=0x{:016X}",
                  dst_addr, size);
        return ERR_INVALID_MEMORY_RANGE;
    }

    return RESULT_SUCCESS;
}

enum class ResourceLimitValueType {
    CurrentValue,
    LimitValue,
};

ResultVal<s64> RetrieveResourceLimitValue(Core::System& system, Handle resource_limit,
                                          u32 resource_type, ResourceLimitValueType value_type) {
    std::lock_guard lock{HLE::g_hle_lock};
    const auto type = static_cast<ResourceType>(resource_type);
    if (!IsValidResourceType(type)) {
        LOG_ERROR(Kernel_SVC, "Invalid resource limit type: '{}'", resource_type);
        return ERR_INVALID_ENUM_VALUE;
    }

    const auto* const current_process = system.Kernel().CurrentProcess();
    ASSERT(current_process != nullptr);

    const auto resource_limit_object =
        current_process->GetHandleTable().Get<ResourceLimit>(resource_limit);
    if (!resource_limit_object) {
        LOG_ERROR(Kernel_SVC, "Handle to non-existent resource limit instance used. Handle={:08X}",
                  resource_limit);
        return ERR_INVALID_HANDLE;
    }

    if (value_type == ResourceLimitValueType::CurrentValue) {
        return MakeResult(resource_limit_object->GetCurrentResourceValue(type));
    }

    return MakeResult(resource_limit_object->GetMaxResourceValue(type));
}
} // Anonymous namespace

/// Set the process heap to a given Size. It can both extend and shrink the heap.
static ResultCode SetHeapSize(Core::System& system, VAddr* heap_addr, u64 heap_size) {
    std::lock_guard lock{HLE::g_hle_lock};
    LOG_TRACE(Kernel_SVC, "called, heap_size=0x{:X}", heap_size);

    // Size must be a multiple of 0x200000 (2MB) and be equal to or less than 8GB.
    if ((heap_size % 0x200000) != 0) {
        LOG_ERROR(Kernel_SVC, "The heap size is not a multiple of 2MB, heap_size=0x{:016X}",
                  heap_size);
        return ERR_INVALID_SIZE;
    }

    if (heap_size >= 0x200000000) {
        LOG_ERROR(Kernel_SVC, "The heap size is not less than 8GB, heap_size=0x{:016X}", heap_size);
        return ERR_INVALID_SIZE;
    }

    auto& page_table{system.Kernel().CurrentProcess()->PageTable()};

    CASCADE_RESULT(*heap_addr, page_table.SetHeapSize(heap_size));

    return RESULT_SUCCESS;
}

static ResultCode SetHeapSize32(Core::System& system, u32* heap_addr, u32 heap_size) {
    VAddr temp_heap_addr{};
    const ResultCode result{SetHeapSize(system, &temp_heap_addr, heap_size)};
    *heap_addr = static_cast<u32>(temp_heap_addr);
    return result;
}

static ResultCode SetMemoryAttribute(Core::System& system, VAddr address, u64 size, u32 mask,
                                     u32 attribute) {
    std::lock_guard lock{HLE::g_hle_lock};
    LOG_DEBUG(Kernel_SVC,
              "called, address=0x{:016X}, size=0x{:X}, mask=0x{:08X}, attribute=0x{:08X}", address,
              size, mask, attribute);

    if (!Common::Is4KBAligned(address)) {
        LOG_ERROR(Kernel_SVC, "Address not page aligned (0x{:016X})", address);
        return ERR_INVALID_ADDRESS;
    }

    if (size == 0 || !Common::Is4KBAligned(size)) {
        LOG_ERROR(Kernel_SVC, "Invalid size (0x{:X}). Size must be non-zero and page aligned.",
                  size);
        return ERR_INVALID_ADDRESS;
    }

    if (!IsValidAddressRange(address, size)) {
        LOG_ERROR(Kernel_SVC, "Address range overflowed (Address: 0x{:016X}, Size: 0x{:016X})",
                  address, size);
        return ERR_INVALID_ADDRESS_STATE;
    }

    const auto attributes{static_cast<Memory::MemoryAttribute>(mask | attribute)};
    if (attributes != static_cast<Memory::MemoryAttribute>(mask) ||
        (attributes | Memory::MemoryAttribute::Uncached) != Memory::MemoryAttribute::Uncached) {
        LOG_ERROR(Kernel_SVC,
                  "Memory attribute doesn't match the given mask (Attribute: 0x{:X}, Mask: {:X}",
                  attribute, mask);
        return ERR_INVALID_COMBINATION;
    }

    auto& page_table{system.Kernel().CurrentProcess()->PageTable()};

    return page_table.SetMemoryAttribute(address, size, static_cast<Memory::MemoryAttribute>(mask),
                                         static_cast<Memory::MemoryAttribute>(attribute));
}

static ResultCode SetMemoryAttribute32(Core::System& system, u32 address, u32 size, u32 mask,
                                       u32 attribute) {
    return SetMemoryAttribute(system, static_cast<VAddr>(address), static_cast<std::size_t>(size),
                              mask, attribute);
}

/// Maps a memory range into a different range.
static ResultCode MapMemory(Core::System& system, VAddr dst_addr, VAddr src_addr, u64 size) {
    std::lock_guard lock{HLE::g_hle_lock};
    LOG_TRACE(Kernel_SVC, "called, dst_addr=0x{:X}, src_addr=0x{:X}, size=0x{:X}", dst_addr,
              src_addr, size);

    auto& page_table{system.Kernel().CurrentProcess()->PageTable()};

    if (const ResultCode result{MapUnmapMemorySanityChecks(page_table, dst_addr, src_addr, size)};
        result.IsError()) {
        return result;
    }

    return page_table.Map(dst_addr, src_addr, size);
}

static ResultCode MapMemory32(Core::System& system, u32 dst_addr, u32 src_addr, u32 size) {
    return MapMemory(system, static_cast<VAddr>(dst_addr), static_cast<VAddr>(src_addr),
                     static_cast<std::size_t>(size));
}

/// Unmaps a region that was previously mapped with svcMapMemory
static ResultCode UnmapMemory(Core::System& system, VAddr dst_addr, VAddr src_addr, u64 size) {
    std::lock_guard lock{HLE::g_hle_lock};
    LOG_TRACE(Kernel_SVC, "called, dst_addr=0x{:X}, src_addr=0x{:X}, size=0x{:X}", dst_addr,
              src_addr, size);

    auto& page_table{system.Kernel().CurrentProcess()->PageTable()};

    if (const ResultCode result{MapUnmapMemorySanityChecks(page_table, dst_addr, src_addr, size)};
        result.IsError()) {
        return result;
    }

    return page_table.Unmap(dst_addr, src_addr, size);
}

static ResultCode UnmapMemory32(Core::System& system, u32 dst_addr, u32 src_addr, u32 size) {
    return UnmapMemory(system, static_cast<VAddr>(dst_addr), static_cast<VAddr>(src_addr),
                       static_cast<std::size_t>(size));
}

/// Connect to an OS service given the port name, returns the handle to the port to out
static ResultCode ConnectToNamedPort(Core::System& system, Handle* out_handle,
                                     VAddr port_name_address) {
    std::lock_guard lock{HLE::g_hle_lock};
    auto& memory = system.Memory();

    if (!memory.IsValidVirtualAddress(port_name_address)) {
        LOG_ERROR(Kernel_SVC,
                  "Port Name Address is not a valid virtual address, port_name_address=0x{:016X}",
                  port_name_address);
        return ERR_NOT_FOUND;
    }

    static constexpr std::size_t PortNameMaxLength = 11;
    // Read 1 char beyond the max allowed port name to detect names that are too long.
    const std::string port_name = memory.ReadCString(port_name_address, PortNameMaxLength + 1);
    if (port_name.size() > PortNameMaxLength) {
        LOG_ERROR(Kernel_SVC, "Port name is too long, expected {} but got {}", PortNameMaxLength,
                  port_name.size());
        return ERR_OUT_OF_RANGE;
    }

    LOG_TRACE(Kernel_SVC, "called port_name={}", port_name);

    auto& kernel = system.Kernel();
    const auto it = kernel.FindNamedPort(port_name);
    if (!kernel.IsValidNamedPort(it)) {
        LOG_WARNING(Kernel_SVC, "tried to connect to unknown port: {}", port_name);
        return ERR_NOT_FOUND;
    }

    ASSERT(kernel.CurrentProcess()->GetResourceLimit()->Reserve(ResourceType::Sessions, 1));

    auto client_port = it->second;

    std::shared_ptr<ClientSession> client_session;
    CASCADE_RESULT(client_session, client_port->Connect());

    // Return the client session
    auto& handle_table = kernel.CurrentProcess()->GetHandleTable();
    CASCADE_RESULT(*out_handle, handle_table.Create(client_session));
    return RESULT_SUCCESS;
}

static ResultCode ConnectToNamedPort32(Core::System& system, Handle* out_handle,
                                       u32 port_name_address) {

    return ConnectToNamedPort(system, out_handle, port_name_address);
}

/// Makes a blocking IPC call to an OS service.
static ResultCode SendSyncRequest(Core::System& system, Handle handle) {
    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    std::shared_ptr<ClientSession> session = handle_table.Get<ClientSession>(handle);
    if (!session) {
        LOG_ERROR(Kernel_SVC, "called with invalid handle=0x{:08X}", handle);
        return ERR_INVALID_HANDLE;
    }

    LOG_TRACE(Kernel_SVC, "called handle=0x{:08X}({})", handle, session->GetName());

    auto thread = system.CurrentScheduler().GetCurrentThread();
    {
        SchedulerLock lock(system.Kernel());
        thread->InvalidateHLECallback();
        thread->SetStatus(ThreadStatus::WaitIPC);
        session->SendSyncRequest(SharedFrom(thread), system.Memory(), system.CoreTiming());
    }

    if (thread->HasHLECallback()) {
        Handle event_handle = thread->GetHLETimeEvent();
        if (event_handle != InvalidHandle) {
            auto& time_manager = system.Kernel().TimeManager();
            time_manager.UnscheduleTimeEvent(event_handle);
        }

        {
            SchedulerLock lock(system.Kernel());
            auto* sync_object = thread->GetHLESyncObject();
            sync_object->RemoveWaitingThread(SharedFrom(thread));
        }

        thread->InvokeHLECallback(SharedFrom(thread));
    }

    return thread->GetSignalingResult();
}

static ResultCode SendSyncRequest32(Core::System& system, Handle handle) {
    return SendSyncRequest(system, handle);
}

/// Get the ID for the specified thread.
static ResultCode GetThreadId(Core::System& system, u64* thread_id, Handle thread_handle) {
    LOG_TRACE(Kernel_SVC, "called thread=0x{:08X}", thread_handle);

    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    const std::shared_ptr<Thread> thread = handle_table.Get<Thread>(thread_handle);
    if (!thread) {
        LOG_ERROR(Kernel_SVC, "Thread handle does not exist, handle=0x{:08X}", thread_handle);
        return ERR_INVALID_HANDLE;
    }

    *thread_id = thread->GetThreadID();
    return RESULT_SUCCESS;
}

static ResultCode GetThreadId32(Core::System& system, u32* thread_id_low, u32* thread_id_high,
                                Handle thread_handle) {
    u64 thread_id{};
    const ResultCode result{GetThreadId(system, &thread_id, thread_handle)};

    *thread_id_low = static_cast<u32>(thread_id >> 32);
    *thread_id_high = static_cast<u32>(thread_id & std::numeric_limits<u32>::max());

    return result;
}

/// Gets the ID of the specified process or a specified thread's owning process.
static ResultCode GetProcessId(Core::System& system, u64* process_id, Handle handle) {
    LOG_DEBUG(Kernel_SVC, "called handle=0x{:08X}", handle);

    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    const std::shared_ptr<Process> process = handle_table.Get<Process>(handle);
    if (process) {
        *process_id = process->GetProcessID();
        return RESULT_SUCCESS;
    }

    const std::shared_ptr<Thread> thread = handle_table.Get<Thread>(handle);
    if (thread) {
        const Process* const owner_process = thread->GetOwnerProcess();
        if (!owner_process) {
            LOG_ERROR(Kernel_SVC, "Non-existent owning process encountered.");
            return ERR_INVALID_HANDLE;
        }

        *process_id = owner_process->GetProcessID();
        return RESULT_SUCCESS;
    }

    // NOTE: This should also handle debug objects before returning.

    LOG_ERROR(Kernel_SVC, "Handle does not exist, handle=0x{:08X}", handle);
    return ERR_INVALID_HANDLE;
}

static ResultCode GetProcessId32(Core::System& system, u32* process_id_low, u32* process_id_high,
                                 Handle handle) {
    u64 process_id{};
    const auto result = GetProcessId(system, &process_id, handle);
    *process_id_low = static_cast<u32>(process_id);
    *process_id_high = static_cast<u32>(process_id >> 32);
    return result;
}

/// Wait for the given handles to synchronize, timeout after the specified nanoseconds
static ResultCode WaitSynchronization(Core::System& system, Handle* index, VAddr handles_address,
                                      u64 handle_count, s64 nano_seconds) {
    LOG_TRACE(Kernel_SVC, "called handles_address=0x{:X}, handle_count={}, nano_seconds={}",
              handles_address, handle_count, nano_seconds);

    auto& memory = system.Memory();
    if (!memory.IsValidVirtualAddress(handles_address)) {
        LOG_ERROR(Kernel_SVC,
                  "Handle address is not a valid virtual address, handle_address=0x{:016X}",
                  handles_address);
        return ERR_INVALID_POINTER;
    }

    static constexpr u64 MaxHandles = 0x40;

    if (handle_count > MaxHandles) {
        LOG_ERROR(Kernel_SVC, "Handle count specified is too large, expected {} but got {}",
                  MaxHandles, handle_count);
        return ERR_OUT_OF_RANGE;
    }

    auto& kernel = system.Kernel();
    Thread::ThreadSynchronizationObjects objects(handle_count);
    const auto& handle_table = kernel.CurrentProcess()->GetHandleTable();

    for (u64 i = 0; i < handle_count; ++i) {
        const Handle handle = memory.Read32(handles_address + i * sizeof(Handle));
        const auto object = handle_table.Get<SynchronizationObject>(handle);

        if (object == nullptr) {
            LOG_ERROR(Kernel_SVC, "Object is a nullptr");
            return ERR_INVALID_HANDLE;
        }

        objects[i] = object;
    }
    auto& synchronization = kernel.Synchronization();
    const auto [result, handle_result] = synchronization.WaitFor(objects, nano_seconds);
    *index = handle_result;
    return result;
}

static ResultCode WaitSynchronization32(Core::System& system, u32 timeout_low, u32 handles_address,
                                        s32 handle_count, u32 timeout_high, Handle* index) {
    const s64 nano_seconds{(static_cast<s64>(timeout_high) << 32) | static_cast<s64>(timeout_low)};
    return WaitSynchronization(system, index, handles_address, handle_count, nano_seconds);
}

/// Resumes a thread waiting on WaitSynchronization
static ResultCode CancelSynchronization(Core::System& system, Handle thread_handle) {
    LOG_TRACE(Kernel_SVC, "called thread=0x{:X}", thread_handle);

    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    std::shared_ptr<Thread> thread = handle_table.Get<Thread>(thread_handle);
    if (!thread) {
        LOG_ERROR(Kernel_SVC, "Thread handle does not exist, thread_handle=0x{:08X}",
                  thread_handle);
        return ERR_INVALID_HANDLE;
    }

    thread->CancelWait();
    return RESULT_SUCCESS;
}

static ResultCode CancelSynchronization32(Core::System& system, Handle thread_handle) {
    return CancelSynchronization(system, thread_handle);
}

/// Attempts to locks a mutex, creating it if it does not already exist
static ResultCode ArbitrateLock(Core::System& system, Handle holding_thread_handle,
                                VAddr mutex_addr, Handle requesting_thread_handle) {
    LOG_TRACE(Kernel_SVC,
              "called holding_thread_handle=0x{:08X}, mutex_addr=0x{:X}, "
              "requesting_current_thread_handle=0x{:08X}",
              holding_thread_handle, mutex_addr, requesting_thread_handle);

    if (Core::Memory::IsKernelVirtualAddress(mutex_addr)) {
        LOG_ERROR(Kernel_SVC, "Mutex Address is a kernel virtual address, mutex_addr={:016X}",
                  mutex_addr);
        return ERR_INVALID_ADDRESS_STATE;
    }

    if (!Common::IsWordAligned(mutex_addr)) {
        LOG_ERROR(Kernel_SVC, "Mutex Address is not word aligned, mutex_addr={:016X}", mutex_addr);
        return ERR_INVALID_ADDRESS;
    }

    auto* const current_process = system.Kernel().CurrentProcess();
    return current_process->GetMutex().TryAcquire(mutex_addr, holding_thread_handle,
                                                  requesting_thread_handle);
}

static ResultCode ArbitrateLock32(Core::System& system, Handle holding_thread_handle,
                                  u32 mutex_addr, Handle requesting_thread_handle) {
    return ArbitrateLock(system, holding_thread_handle, static_cast<VAddr>(mutex_addr),
                         requesting_thread_handle);
}

/// Unlock a mutex
static ResultCode ArbitrateUnlock(Core::System& system, VAddr mutex_addr) {
    LOG_TRACE(Kernel_SVC, "called mutex_addr=0x{:X}", mutex_addr);

    if (Core::Memory::IsKernelVirtualAddress(mutex_addr)) {
        LOG_ERROR(Kernel_SVC, "Mutex Address is a kernel virtual address, mutex_addr={:016X}",
                  mutex_addr);
        return ERR_INVALID_ADDRESS_STATE;
    }

    if (!Common::IsWordAligned(mutex_addr)) {
        LOG_ERROR(Kernel_SVC, "Mutex Address is not word aligned, mutex_addr={:016X}", mutex_addr);
        return ERR_INVALID_ADDRESS;
    }

    auto* const current_process = system.Kernel().CurrentProcess();
    return current_process->GetMutex().Release(mutex_addr);
}

static ResultCode ArbitrateUnlock32(Core::System& system, u32 mutex_addr) {
    return ArbitrateUnlock(system, static_cast<VAddr>(mutex_addr));
}

enum class BreakType : u32 {
    Panic = 0,
    AssertionFailed = 1,
    PreNROLoad = 3,
    PostNROLoad = 4,
    PreNROUnload = 5,
    PostNROUnload = 6,
    CppException = 7,
};

struct BreakReason {
    union {
        u32 raw;
        BitField<0, 30, BreakType> break_type;
        BitField<31, 1, u32> signal_debugger;
    };
};

/// Break program execution
static void Break(Core::System& system, u32 reason, u64 info1, u64 info2) {
    BreakReason break_reason{reason};
    bool has_dumped_buffer{};
    std::vector<u8> debug_buffer;

    const auto handle_debug_buffer = [&](VAddr addr, u64 sz) {
        if (sz == 0 || addr == 0 || has_dumped_buffer) {
            return;
        }

        auto& memory = system.Memory();

        // This typically is an error code so we're going to assume this is the case
        if (sz == sizeof(u32)) {
            LOG_CRITICAL(Debug_Emulated, "debug_buffer_err_code={:X}", memory.Read32(addr));
        } else {
            // We don't know what's in here so we'll hexdump it
            debug_buffer.resize(sz);
            memory.ReadBlock(addr, debug_buffer.data(), sz);
            std::string hexdump;
            for (std::size_t i = 0; i < debug_buffer.size(); i++) {
                hexdump += fmt::format("{:02X} ", debug_buffer[i]);
                if (i != 0 && i % 16 == 0) {
                    hexdump += '\n';
                }
            }
            LOG_CRITICAL(Debug_Emulated, "debug_buffer=\n{}", hexdump);
        }
        has_dumped_buffer = true;
    };
    switch (break_reason.break_type) {
    case BreakType::Panic:
        LOG_CRITICAL(Debug_Emulated, "Signalling debugger, PANIC! info1=0x{:016X}, info2=0x{:016X}",
                     info1, info2);
        handle_debug_buffer(info1, info2);
        break;
    case BreakType::AssertionFailed:
        LOG_CRITICAL(Debug_Emulated,
                     "Signalling debugger, Assertion failed! info1=0x{:016X}, info2=0x{:016X}",
                     info1, info2);
        handle_debug_buffer(info1, info2);
        break;
    case BreakType::PreNROLoad:
        LOG_WARNING(
            Debug_Emulated,
            "Signalling debugger, Attempting to load an NRO at 0x{:016X} with size 0x{:016X}",
            info1, info2);
        break;
    case BreakType::PostNROLoad:
        LOG_WARNING(Debug_Emulated,
                    "Signalling debugger, Loaded an NRO at 0x{:016X} with size 0x{:016X}", info1,
                    info2);
        break;
    case BreakType::PreNROUnload:
        LOG_WARNING(
            Debug_Emulated,
            "Signalling debugger, Attempting to unload an NRO at 0x{:016X} with size 0x{:016X}",
            info1, info2);
        break;
    case BreakType::PostNROUnload:
        LOG_WARNING(Debug_Emulated,
                    "Signalling debugger, Unloaded an NRO at 0x{:016X} with size 0x{:016X}", info1,
                    info2);
        break;
    case BreakType::CppException:
        LOG_CRITICAL(Debug_Emulated, "Signalling debugger. Uncaught C++ exception encountered.");
        break;
    default:
        LOG_WARNING(
            Debug_Emulated,
            "Signalling debugger, Unknown break reason {}, info1=0x{:016X}, info2=0x{:016X}",
            static_cast<u32>(break_reason.break_type.Value()), info1, info2);
        handle_debug_buffer(info1, info2);
        break;
    }

    system.GetReporter().SaveSvcBreakReport(
        static_cast<u32>(break_reason.break_type.Value()), break_reason.signal_debugger, info1,
        info2, has_dumped_buffer ? std::make_optional(debug_buffer) : std::nullopt);

    if (!break_reason.signal_debugger) {
        SchedulerLock lock(system.Kernel());
        LOG_CRITICAL(
            Debug_Emulated,
            "Emulated program broke execution! reason=0x{:016X}, info1=0x{:016X}, info2=0x{:016X}",
            reason, info1, info2);

        handle_debug_buffer(info1, info2);

        auto* const current_thread = system.CurrentScheduler().GetCurrentThread();
        const auto thread_processor_id = current_thread->GetProcessorID();
        system.ArmInterface(static_cast<std::size_t>(thread_processor_id)).LogBacktrace();

        // Kill the current thread
        system.Kernel().ExceptionalExit();
        current_thread->Stop();
    }
}

static void Break32(Core::System& system, u32 reason, u32 info1, u32 info2) {
    Break(system, reason, static_cast<u64>(info1), static_cast<u64>(info2));
}

/// Used to output a message on a debug hardware unit - does nothing on a retail unit
static void OutputDebugString([[maybe_unused]] Core::System& system, VAddr address, u64 len) {
    if (len == 0) {
        return;
    }

    std::string str(len, '\0');
    system.Memory().ReadBlock(address, str.data(), str.size());
    LOG_DEBUG(Debug_Emulated, "{}", str);
}

/// Gets system/memory information for the current process
static ResultCode GetInfo(Core::System& system, u64* result, u64 info_id, u64 handle,
                          u64 info_sub_id) {
    std::lock_guard lock{HLE::g_hle_lock};
    LOG_TRACE(Kernel_SVC, "called info_id=0x{:X}, info_sub_id=0x{:X}, handle=0x{:08X}", info_id,
              info_sub_id, handle);

    enum class GetInfoType : u64 {
        // 1.0.0+
        AllowedCPUCoreMask = 0,
        AllowedThreadPriorityMask = 1,
        MapRegionBaseAddr = 2,
        MapRegionSize = 3,
        HeapRegionBaseAddr = 4,
        HeapRegionSize = 5,
        TotalPhysicalMemoryAvailable = 6,
        TotalPhysicalMemoryUsed = 7,
        IsCurrentProcessBeingDebugged = 8,
        RegisterResourceLimit = 9,
        IdleTickCount = 10,
        RandomEntropy = 11,
        ThreadTickCount = 0xF0000002,
        // 2.0.0+
        ASLRRegionBaseAddr = 12,
        ASLRRegionSize = 13,
        StackRegionBaseAddr = 14,
        StackRegionSize = 15,
        // 3.0.0+
        SystemResourceSize = 16,
        SystemResourceUsage = 17,
        TitleId = 18,
        // 4.0.0+
        PrivilegedProcessId = 19,
        // 5.0.0+
        UserExceptionContextAddr = 20,
        // 6.0.0+
        TotalPhysicalMemoryAvailableWithoutSystemResource = 21,
        TotalPhysicalMemoryUsedWithoutSystemResource = 22,
    };

    const auto info_id_type = static_cast<GetInfoType>(info_id);

    switch (info_id_type) {
    case GetInfoType::AllowedCPUCoreMask:
    case GetInfoType::AllowedThreadPriorityMask:
    case GetInfoType::MapRegionBaseAddr:
    case GetInfoType::MapRegionSize:
    case GetInfoType::HeapRegionBaseAddr:
    case GetInfoType::HeapRegionSize:
    case GetInfoType::ASLRRegionBaseAddr:
    case GetInfoType::ASLRRegionSize:
    case GetInfoType::StackRegionBaseAddr:
    case GetInfoType::StackRegionSize:
    case GetInfoType::TotalPhysicalMemoryAvailable:
    case GetInfoType::TotalPhysicalMemoryUsed:
    case GetInfoType::SystemResourceSize:
    case GetInfoType::SystemResourceUsage:
    case GetInfoType::TitleId:
    case GetInfoType::UserExceptionContextAddr:
    case GetInfoType::TotalPhysicalMemoryAvailableWithoutSystemResource:
    case GetInfoType::TotalPhysicalMemoryUsedWithoutSystemResource: {
        if (info_sub_id != 0) {
            LOG_ERROR(Kernel_SVC, "Info sub id is non zero! info_id={}, info_sub_id={}", info_id,
                      info_sub_id);
            return ERR_INVALID_ENUM_VALUE;
        }

        const auto& current_process_handle_table =
            system.Kernel().CurrentProcess()->GetHandleTable();
        const auto process = current_process_handle_table.Get<Process>(static_cast<Handle>(handle));
        if (!process) {
            LOG_ERROR(Kernel_SVC, "Process is not valid! info_id={}, info_sub_id={}, handle={:08X}",
                      info_id, info_sub_id, handle);
            return ERR_INVALID_HANDLE;
        }

        switch (info_id_type) {
        case GetInfoType::AllowedCPUCoreMask:
            *result = process->GetCoreMask();
            return RESULT_SUCCESS;

        case GetInfoType::AllowedThreadPriorityMask:
            *result = process->GetPriorityMask();
            return RESULT_SUCCESS;

        case GetInfoType::MapRegionBaseAddr:
            *result = process->PageTable().GetAliasRegionStart();
            return RESULT_SUCCESS;

        case GetInfoType::MapRegionSize:
            *result = process->PageTable().GetAliasRegionSize();
            return RESULT_SUCCESS;

        case GetInfoType::HeapRegionBaseAddr:
            *result = process->PageTable().GetHeapRegionStart();
            return RESULT_SUCCESS;

        case GetInfoType::HeapRegionSize:
            *result = process->PageTable().GetHeapRegionSize();
            return RESULT_SUCCESS;

        case GetInfoType::ASLRRegionBaseAddr:
            *result = process->PageTable().GetAliasCodeRegionStart();
            return RESULT_SUCCESS;

        case GetInfoType::ASLRRegionSize:
            *result = process->PageTable().GetAliasCodeRegionSize();
            return RESULT_SUCCESS;

        case GetInfoType::StackRegionBaseAddr:
            *result = process->PageTable().GetStackRegionStart();
            return RESULT_SUCCESS;

        case GetInfoType::StackRegionSize:
            *result = process->PageTable().GetStackRegionSize();
            return RESULT_SUCCESS;

        case GetInfoType::TotalPhysicalMemoryAvailable:
            *result = process->GetTotalPhysicalMemoryAvailable();
            return RESULT_SUCCESS;

        case GetInfoType::TotalPhysicalMemoryUsed:
            *result = process->GetTotalPhysicalMemoryUsed();
            return RESULT_SUCCESS;

        case GetInfoType::SystemResourceSize:
            *result = process->GetSystemResourceSize();
            return RESULT_SUCCESS;

        case GetInfoType::SystemResourceUsage:
            LOG_WARNING(Kernel_SVC, "(STUBBED) Attempted to query system resource usage");
            *result = process->GetSystemResourceUsage();
            return RESULT_SUCCESS;

        case GetInfoType::TitleId:
            *result = process->GetTitleID();
            return RESULT_SUCCESS;

        case GetInfoType::UserExceptionContextAddr:
            *result = process->GetTLSRegionAddress();
            return RESULT_SUCCESS;

        case GetInfoType::TotalPhysicalMemoryAvailableWithoutSystemResource:
            *result = process->GetTotalPhysicalMemoryAvailableWithoutSystemResource();
            return RESULT_SUCCESS;

        case GetInfoType::TotalPhysicalMemoryUsedWithoutSystemResource:
            *result = process->GetTotalPhysicalMemoryUsedWithoutSystemResource();
            return RESULT_SUCCESS;

        default:
            break;
        }

        LOG_ERROR(Kernel_SVC, "Unimplemented svcGetInfo id=0x{:016X}", info_id);
        return ERR_INVALID_ENUM_VALUE;
    }

    case GetInfoType::IsCurrentProcessBeingDebugged:
        *result = 0;
        return RESULT_SUCCESS;

    case GetInfoType::RegisterResourceLimit: {
        if (handle != 0) {
            LOG_ERROR(Kernel, "Handle is non zero! handle={:08X}", handle);
            return ERR_INVALID_HANDLE;
        }

        if (info_sub_id != 0) {
            LOG_ERROR(Kernel, "Info sub id is non zero! info_id={}, info_sub_id={}", info_id,
                      info_sub_id);
            return ERR_INVALID_COMBINATION;
        }

        Process* const current_process = system.Kernel().CurrentProcess();
        HandleTable& handle_table = current_process->GetHandleTable();
        const auto resource_limit = current_process->GetResourceLimit();
        if (!resource_limit) {
            *result = KernelHandle::InvalidHandle;
            // Yes, the kernel considers this a successful operation.
            return RESULT_SUCCESS;
        }

        const auto table_result = handle_table.Create(resource_limit);
        if (table_result.Failed()) {
            return table_result.Code();
        }

        *result = *table_result;
        return RESULT_SUCCESS;
    }

    case GetInfoType::RandomEntropy:
        if (handle != 0) {
            LOG_ERROR(Kernel_SVC, "Process Handle is non zero, expected 0 result but got {:016X}",
                      handle);
            return ERR_INVALID_HANDLE;
        }

        if (info_sub_id >= Process::RANDOM_ENTROPY_SIZE) {
            LOG_ERROR(Kernel_SVC, "Entropy size is out of range, expected {} but got {}",
                      Process::RANDOM_ENTROPY_SIZE, info_sub_id);
            return ERR_INVALID_COMBINATION;
        }

        *result = system.Kernel().CurrentProcess()->GetRandomEntropy(info_sub_id);
        return RESULT_SUCCESS;

    case GetInfoType::PrivilegedProcessId:
        LOG_WARNING(Kernel_SVC,
                    "(STUBBED) Attempted to query privileged process id bounds, returned 0");
        *result = 0;
        return RESULT_SUCCESS;

    case GetInfoType::ThreadTickCount: {
        constexpr u64 num_cpus = 4;
        if (info_sub_id != 0xFFFFFFFFFFFFFFFF && info_sub_id >= num_cpus) {
            LOG_ERROR(Kernel_SVC, "Core count is out of range, expected {} but got {}", num_cpus,
                      info_sub_id);
            return ERR_INVALID_COMBINATION;
        }

        const auto thread = system.Kernel().CurrentProcess()->GetHandleTable().Get<Thread>(
            static_cast<Handle>(handle));
        if (!thread) {
            LOG_ERROR(Kernel_SVC, "Thread handle does not exist, handle=0x{:08X}",
                      static_cast<Handle>(handle));
            return ERR_INVALID_HANDLE;
        }

        const auto& core_timing = system.CoreTiming();
        const auto& scheduler = system.CurrentScheduler();
        const auto* const current_thread = scheduler.GetCurrentThread();
        const bool same_thread = current_thread == thread.get();

        const u64 prev_ctx_ticks = scheduler.GetLastContextSwitchTicks();
        u64 out_ticks = 0;
        if (same_thread && info_sub_id == 0xFFFFFFFFFFFFFFFF) {
            const u64 thread_ticks = current_thread->GetTotalCPUTimeTicks();

            out_ticks = thread_ticks + (core_timing.GetCPUTicks() - prev_ctx_ticks);
        } else if (same_thread && info_sub_id == system.CurrentCoreIndex()) {
            out_ticks = core_timing.GetCPUTicks() - prev_ctx_ticks;
        }

        *result = out_ticks;
        return RESULT_SUCCESS;
    }

    default:
        LOG_ERROR(Kernel_SVC, "Unimplemented svcGetInfo id=0x{:016X}", info_id);
        return ERR_INVALID_ENUM_VALUE;
    }
}

static ResultCode GetInfo32(Core::System& system, u32* result_low, u32* result_high, u32 sub_id_low,
                            u32 info_id, u32 handle, u32 sub_id_high) {
    const u64 sub_id{static_cast<u64>(sub_id_low | (static_cast<u64>(sub_id_high) << 32))};
    u64 res_value{};

    const ResultCode result{GetInfo(system, &res_value, info_id, handle, sub_id)};
    *result_high = static_cast<u32>(res_value >> 32);
    *result_low = static_cast<u32>(res_value & std::numeric_limits<u32>::max());

    return result;
}

/// Maps memory at a desired address
static ResultCode MapPhysicalMemory(Core::System& system, VAddr addr, u64 size) {
    std::lock_guard lock{HLE::g_hle_lock};
    LOG_DEBUG(Kernel_SVC, "called, addr=0x{:016X}, size=0x{:X}", addr, size);

    if (!Common::Is4KBAligned(addr)) {
        LOG_ERROR(Kernel_SVC, "Address is not aligned to 4KB, 0x{:016X}", addr);
        return ERR_INVALID_ADDRESS;
    }

    if (!Common::Is4KBAligned(size)) {
        LOG_ERROR(Kernel_SVC, "Size is not aligned to 4KB, 0x{:X}", size);
        return ERR_INVALID_SIZE;
    }

    if (size == 0) {
        LOG_ERROR(Kernel_SVC, "Size is zero");
        return ERR_INVALID_SIZE;
    }

    if (!(addr < addr + size)) {
        LOG_ERROR(Kernel_SVC, "Size causes 64-bit overflow of address");
        return ERR_INVALID_MEMORY_RANGE;
    }

    Process* const current_process{system.Kernel().CurrentProcess()};
    auto& page_table{current_process->PageTable()};

    if (current_process->GetSystemResourceSize() == 0) {
        LOG_ERROR(Kernel_SVC, "System Resource Size is zero");
        return ERR_INVALID_STATE;
    }

    if (!page_table.IsInsideAddressSpace(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Address is not within the address space, addr=0x{:016X}, size=0x{:016X}", addr,
                  size);
        return ERR_INVALID_MEMORY_RANGE;
    }

    if (page_table.IsOutsideAliasRegion(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Address is not within the alias region, addr=0x{:016X}, size=0x{:016X}", addr,
                  size);
        return ERR_INVALID_MEMORY_RANGE;
    }

    return page_table.MapPhysicalMemory(addr, size);
}

static ResultCode MapPhysicalMemory32(Core::System& system, u32 addr, u32 size) {
    return MapPhysicalMemory(system, static_cast<VAddr>(addr), static_cast<std::size_t>(size));
}

/// Unmaps memory previously mapped via MapPhysicalMemory
static ResultCode UnmapPhysicalMemory(Core::System& system, VAddr addr, u64 size) {
    std::lock_guard lock{HLE::g_hle_lock};
    LOG_DEBUG(Kernel_SVC, "called, addr=0x{:016X}, size=0x{:X}", addr, size);

    if (!Common::Is4KBAligned(addr)) {
        LOG_ERROR(Kernel_SVC, "Address is not aligned to 4KB, 0x{:016X}", addr);
        return ERR_INVALID_ADDRESS;
    }

    if (!Common::Is4KBAligned(size)) {
        LOG_ERROR(Kernel_SVC, "Size is not aligned to 4KB, 0x{:X}", size);
        return ERR_INVALID_SIZE;
    }

    if (size == 0) {
        LOG_ERROR(Kernel_SVC, "Size is zero");
        return ERR_INVALID_SIZE;
    }

    if (!(addr < addr + size)) {
        LOG_ERROR(Kernel_SVC, "Size causes 64-bit overflow of address");
        return ERR_INVALID_MEMORY_RANGE;
    }

    Process* const current_process{system.Kernel().CurrentProcess()};
    auto& page_table{current_process->PageTable()};

    if (current_process->GetSystemResourceSize() == 0) {
        LOG_ERROR(Kernel_SVC, "System Resource Size is zero");
        return ERR_INVALID_STATE;
    }

    if (!page_table.IsInsideAddressSpace(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Address is not within the address space, addr=0x{:016X}, size=0x{:016X}", addr,
                  size);
        return ERR_INVALID_MEMORY_RANGE;
    }

    if (page_table.IsOutsideAliasRegion(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Address is not within the alias region, addr=0x{:016X}, size=0x{:016X}", addr,
                  size);
        return ERR_INVALID_MEMORY_RANGE;
    }

    return page_table.UnmapPhysicalMemory(addr, size);
}

static ResultCode UnmapPhysicalMemory32(Core::System& system, u32 addr, u32 size) {
    return UnmapPhysicalMemory(system, static_cast<VAddr>(addr), static_cast<std::size_t>(size));
}

/// Sets the thread activity
static ResultCode SetThreadActivity(Core::System& system, Handle handle, u32 activity) {
    LOG_DEBUG(Kernel_SVC, "called, handle=0x{:08X}, activity=0x{:08X}", handle, activity);
    if (activity > static_cast<u32>(ThreadActivity::Paused)) {
        return ERR_INVALID_ENUM_VALUE;
    }

    const auto* current_process = system.Kernel().CurrentProcess();
    const std::shared_ptr<Thread> thread = current_process->GetHandleTable().Get<Thread>(handle);
    if (!thread) {
        LOG_ERROR(Kernel_SVC, "Thread handle does not exist, handle=0x{:08X}", handle);
        return ERR_INVALID_HANDLE;
    }

    if (thread->GetOwnerProcess() != current_process) {
        LOG_ERROR(Kernel_SVC,
                  "The current process does not own the current thread, thread_handle={:08X} "
                  "thread_pid={}, "
                  "current_process_pid={}",
                  handle, thread->GetOwnerProcess()->GetProcessID(),
                  current_process->GetProcessID());
        return ERR_INVALID_HANDLE;
    }

    if (thread.get() == system.CurrentScheduler().GetCurrentThread()) {
        LOG_ERROR(Kernel_SVC, "The thread handle specified is the current running thread");
        return ERR_BUSY;
    }

    return thread->SetActivity(static_cast<ThreadActivity>(activity));
}

static ResultCode SetThreadActivity32(Core::System& system, Handle handle, u32 activity) {
    return SetThreadActivity(system, handle, activity);
}

/// Gets the thread context
static ResultCode GetThreadContext(Core::System& system, VAddr thread_context, Handle handle) {
    LOG_DEBUG(Kernel_SVC, "called, context=0x{:08X}, thread=0x{:X}", thread_context, handle);

    const auto* current_process = system.Kernel().CurrentProcess();
    const std::shared_ptr<Thread> thread = current_process->GetHandleTable().Get<Thread>(handle);
    if (!thread) {
        LOG_ERROR(Kernel_SVC, "Thread handle does not exist, handle=0x{:08X}", handle);
        return ERR_INVALID_HANDLE;
    }

    if (thread->GetOwnerProcess() != current_process) {
        LOG_ERROR(Kernel_SVC,
                  "The current process does not own the current thread, thread_handle={:08X} "
                  "thread_pid={}, "
                  "current_process_pid={}",
                  handle, thread->GetOwnerProcess()->GetProcessID(),
                  current_process->GetProcessID());
        return ERR_INVALID_HANDLE;
    }

    if (thread.get() == system.CurrentScheduler().GetCurrentThread()) {
        LOG_ERROR(Kernel_SVC, "The thread handle specified is the current running thread");
        return ERR_BUSY;
    }

    Core::ARM_Interface::ThreadContext64 ctx = thread->GetContext64();
    // Mask away mode bits, interrupt bits, IL bit, and other reserved bits.
    ctx.pstate &= 0xFF0FFE20;

    // If 64-bit, we can just write the context registers directly and we're good.
    // However, if 32-bit, we have to ensure some registers are zeroed out.
    if (!current_process->Is64BitProcess()) {
        std::fill(ctx.cpu_registers.begin() + 15, ctx.cpu_registers.end(), 0);
        std::fill(ctx.vector_registers.begin() + 16, ctx.vector_registers.end(), u128{});
    }

    system.Memory().WriteBlock(thread_context, &ctx, sizeof(ctx));
    return RESULT_SUCCESS;
}

static ResultCode GetThreadContext32(Core::System& system, u32 thread_context, Handle handle) {
    return GetThreadContext(system, static_cast<VAddr>(thread_context), handle);
}

/// Gets the priority for the specified thread
static ResultCode GetThreadPriority(Core::System& system, u32* priority, Handle handle) {
    LOG_TRACE(Kernel_SVC, "called");

    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    const std::shared_ptr<Thread> thread = handle_table.Get<Thread>(handle);
    if (!thread) {
        *priority = 0;
        LOG_ERROR(Kernel_SVC, "Thread handle does not exist, handle=0x{:08X}", handle);
        return ERR_INVALID_HANDLE;
    }

    *priority = thread->GetPriority();
    return RESULT_SUCCESS;
}

static ResultCode GetThreadPriority32(Core::System& system, u32* priority, Handle handle) {
    return GetThreadPriority(system, priority, handle);
}

/// Sets the priority for the specified thread
static ResultCode SetThreadPriority(Core::System& system, Handle handle, u32 priority) {
    LOG_TRACE(Kernel_SVC, "called");

    if (priority > THREADPRIO_LOWEST) {
        LOG_ERROR(
            Kernel_SVC,
            "An invalid priority was specified, expected {} but got {} for thread_handle={:08X}",
            THREADPRIO_LOWEST, priority, handle);
        return ERR_INVALID_THREAD_PRIORITY;
    }

    const auto* const current_process = system.Kernel().CurrentProcess();

    std::shared_ptr<Thread> thread = current_process->GetHandleTable().Get<Thread>(handle);
    if (!thread) {
        LOG_ERROR(Kernel_SVC, "Thread handle does not exist, handle=0x{:08X}", handle);
        return ERR_INVALID_HANDLE;
    }

    thread->SetPriority(priority);

    return RESULT_SUCCESS;
}

static ResultCode SetThreadPriority32(Core::System& system, Handle handle, u32 priority) {
    return SetThreadPriority(system, handle, priority);
}

/// Get which CPU core is executing the current thread
static u32 GetCurrentProcessorNumber(Core::System& system) {
    LOG_TRACE(Kernel_SVC, "called");
    return static_cast<u32>(system.CurrentPhysicalCore().CoreIndex());
}

static u32 GetCurrentProcessorNumber32(Core::System& system) {
    return GetCurrentProcessorNumber(system);
}

static ResultCode MapSharedMemory(Core::System& system, Handle shared_memory_handle, VAddr addr,
                                  u64 size, u32 permissions) {
    std::lock_guard lock{HLE::g_hle_lock};
    LOG_TRACE(Kernel_SVC,
              "called, shared_memory_handle=0x{:X}, addr=0x{:X}, size=0x{:X}, permissions=0x{:08X}",
              shared_memory_handle, addr, size, permissions);

    if (!Common::Is4KBAligned(addr)) {
        LOG_ERROR(Kernel_SVC, "Address is not aligned to 4KB, addr=0x{:016X}", addr);
        return ERR_INVALID_ADDRESS;
    }

    if (size == 0) {
        LOG_ERROR(Kernel_SVC, "Size is 0");
        return ERR_INVALID_SIZE;
    }

    if (!Common::Is4KBAligned(size)) {
        LOG_ERROR(Kernel_SVC, "Size is not aligned to 4KB, size=0x{:016X}", size);
        return ERR_INVALID_SIZE;
    }

    if (!IsValidAddressRange(addr, size)) {
        LOG_ERROR(Kernel_SVC, "Region is not a valid address range, addr=0x{:016X}, size=0x{:016X}",
                  addr, size);
        return ERR_INVALID_ADDRESS_STATE;
    }

    const auto permission_type = static_cast<Memory::MemoryPermission>(permissions);
    if ((permission_type | Memory::MemoryPermission::Write) !=
        Memory::MemoryPermission::ReadAndWrite) {
        LOG_ERROR(Kernel_SVC, "Expected Read or ReadWrite permission but got permissions=0x{:08X}",
                  permissions);
        return ERR_INVALID_MEMORY_PERMISSIONS;
    }

    auto* const current_process{system.Kernel().CurrentProcess()};
    auto& page_table{current_process->PageTable()};

    if (page_table.IsInvalidRegion(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Addr does not fit within the valid region, addr=0x{:016X}, "
                  "size=0x{:016X}",
                  addr, size);
        return ERR_INVALID_MEMORY_RANGE;
    }

    if (page_table.IsInsideHeapRegion(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Addr does not fit within the heap region, addr=0x{:016X}, "
                  "size=0x{:016X}",
                  addr, size);
        return ERR_INVALID_MEMORY_RANGE;
    }

    if (page_table.IsInsideAliasRegion(addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Address does not fit within the map region, addr=0x{:016X}, "
                  "size=0x{:016X}",
                  addr, size);
        return ERR_INVALID_MEMORY_RANGE;
    }

    auto shared_memory{current_process->GetHandleTable().Get<SharedMemory>(shared_memory_handle)};
    if (!shared_memory) {
        LOG_ERROR(Kernel_SVC, "Shared memory does not exist, shared_memory_handle=0x{:08X}",
                  shared_memory_handle);
        return ERR_INVALID_HANDLE;
    }

    return shared_memory->Map(*current_process, addr, size, permission_type);
}

static ResultCode MapSharedMemory32(Core::System& system, Handle shared_memory_handle, u32 addr,
                                    u32 size, u32 permissions) {
    return MapSharedMemory(system, shared_memory_handle, static_cast<VAddr>(addr),
                           static_cast<std::size_t>(size), permissions);
}

static ResultCode QueryProcessMemory(Core::System& system, VAddr memory_info_address,
                                     VAddr page_info_address, Handle process_handle,
                                     VAddr address) {
    std::lock_guard lock{HLE::g_hle_lock};
    LOG_TRACE(Kernel_SVC, "called process=0x{:08X} address={:X}", process_handle, address);
    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    std::shared_ptr<Process> process = handle_table.Get<Process>(process_handle);
    if (!process) {
        LOG_ERROR(Kernel_SVC, "Process handle does not exist, process_handle=0x{:08X}",
                  process_handle);
        return ERR_INVALID_HANDLE;
    }

    auto& memory{system.Memory()};
    const auto memory_info{process->PageTable().QueryInfo(address).GetSvcMemoryInfo()};

    memory.Write64(memory_info_address + 0x00, memory_info.addr);
    memory.Write64(memory_info_address + 0x08, memory_info.size);
    memory.Write32(memory_info_address + 0x10, static_cast<u32>(memory_info.state) & 0xff);
    memory.Write32(memory_info_address + 0x14, static_cast<u32>(memory_info.attr));
    memory.Write32(memory_info_address + 0x18, static_cast<u32>(memory_info.perm));
    memory.Write32(memory_info_address + 0x1c, memory_info.ipc_refcount);
    memory.Write32(memory_info_address + 0x20, memory_info.device_refcount);
    memory.Write32(memory_info_address + 0x24, 0);

    // Page info appears to be currently unused by the kernel and is always set to zero.
    memory.Write32(page_info_address, 0);

    return RESULT_SUCCESS;
}

static ResultCode QueryMemory(Core::System& system, VAddr memory_info_address,
                              VAddr page_info_address, VAddr query_address) {
    LOG_TRACE(Kernel_SVC,
              "called, memory_info_address=0x{:016X}, page_info_address=0x{:016X}, "
              "query_address=0x{:016X}",
              memory_info_address, page_info_address, query_address);

    return QueryProcessMemory(system, memory_info_address, page_info_address, CurrentProcess,
                              query_address);
}

static ResultCode QueryMemory32(Core::System& system, u32 memory_info_address,
                                u32 page_info_address, u32 query_address) {
    return QueryMemory(system, memory_info_address, page_info_address, query_address);
}

static ResultCode MapProcessCodeMemory(Core::System& system, Handle process_handle, u64 dst_address,
                                       u64 src_address, u64 size) {
    LOG_DEBUG(Kernel_SVC,
              "called. process_handle=0x{:08X}, dst_address=0x{:016X}, "
              "src_address=0x{:016X}, size=0x{:016X}",
              process_handle, dst_address, src_address, size);

    if (!Common::Is4KBAligned(src_address)) {
        LOG_ERROR(Kernel_SVC, "src_address is not page-aligned (src_address=0x{:016X}).",
                  src_address);
        return ERR_INVALID_ADDRESS;
    }

    if (!Common::Is4KBAligned(dst_address)) {
        LOG_ERROR(Kernel_SVC, "dst_address is not page-aligned (dst_address=0x{:016X}).",
                  dst_address);
        return ERR_INVALID_ADDRESS;
    }

    if (size == 0 || !Common::Is4KBAligned(size)) {
        LOG_ERROR(Kernel_SVC, "Size is zero or not page-aligned (size=0x{:016X})", size);
        return ERR_INVALID_SIZE;
    }

    if (!IsValidAddressRange(dst_address, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Destination address range overflows the address space (dst_address=0x{:016X}, "
                  "size=0x{:016X}).",
                  dst_address, size);
        return ERR_INVALID_ADDRESS_STATE;
    }

    if (!IsValidAddressRange(src_address, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Source address range overflows the address space (src_address=0x{:016X}, "
                  "size=0x{:016X}).",
                  src_address, size);
        return ERR_INVALID_ADDRESS_STATE;
    }

    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    auto process = handle_table.Get<Process>(process_handle);
    if (!process) {
        LOG_ERROR(Kernel_SVC, "Invalid process handle specified (handle=0x{:08X}).",
                  process_handle);
        return ERR_INVALID_HANDLE;
    }

    auto& page_table = process->PageTable();
    if (!page_table.IsInsideAddressSpace(src_address, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Source address range is not within the address space (src_address=0x{:016X}, "
                  "size=0x{:016X}).",
                  src_address, size);
        return ERR_INVALID_ADDRESS_STATE;
    }

    if (!page_table.IsInsideASLRRegion(dst_address, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Destination address range is not within the ASLR region (dst_address=0x{:016X}, "
                  "size=0x{:016X}).",
                  dst_address, size);
        return ERR_INVALID_MEMORY_RANGE;
    }

    return page_table.MapProcessCodeMemory(dst_address, src_address, size);
}

static ResultCode UnmapProcessCodeMemory(Core::System& system, Handle process_handle,
                                         u64 dst_address, u64 src_address, u64 size) {
    LOG_DEBUG(Kernel_SVC,
              "called. process_handle=0x{:08X}, dst_address=0x{:016X}, src_address=0x{:016X}, "
              "size=0x{:016X}",
              process_handle, dst_address, src_address, size);

    if (!Common::Is4KBAligned(dst_address)) {
        LOG_ERROR(Kernel_SVC, "dst_address is not page-aligned (dst_address=0x{:016X}).",
                  dst_address);
        return ERR_INVALID_ADDRESS;
    }

    if (!Common::Is4KBAligned(src_address)) {
        LOG_ERROR(Kernel_SVC, "src_address is not page-aligned (src_address=0x{:016X}).",
                  src_address);
        return ERR_INVALID_ADDRESS;
    }

    if (size == 0 || Common::Is4KBAligned(size)) {
        LOG_ERROR(Kernel_SVC, "Size is zero or not page-aligned (size=0x{:016X}).", size);
        return ERR_INVALID_SIZE;
    }

    if (!IsValidAddressRange(dst_address, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Destination address range overflows the address space (dst_address=0x{:016X}, "
                  "size=0x{:016X}).",
                  dst_address, size);
        return ERR_INVALID_ADDRESS_STATE;
    }

    if (!IsValidAddressRange(src_address, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Source address range overflows the address space (src_address=0x{:016X}, "
                  "size=0x{:016X}).",
                  src_address, size);
        return ERR_INVALID_ADDRESS_STATE;
    }

    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    auto process = handle_table.Get<Process>(process_handle);
    if (!process) {
        LOG_ERROR(Kernel_SVC, "Invalid process handle specified (handle=0x{:08X}).",
                  process_handle);
        return ERR_INVALID_HANDLE;
    }

    auto& page_table = process->PageTable();
    if (!page_table.IsInsideAddressSpace(src_address, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Source address range is not within the address space (src_address=0x{:016X}, "
                  "size=0x{:016X}).",
                  src_address, size);
        return ERR_INVALID_ADDRESS_STATE;
    }

    if (!page_table.IsInsideASLRRegion(dst_address, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Destination address range is not within the ASLR region (dst_address=0x{:016X}, "
                  "size=0x{:016X}).",
                  dst_address, size);
        return ERR_INVALID_MEMORY_RANGE;
    }

    return page_table.UnmapProcessCodeMemory(dst_address, src_address, size);
}

/// Exits the current process
static void ExitProcess(Core::System& system) {
    auto* current_process = system.Kernel().CurrentProcess();
    UNIMPLEMENTED();

    LOG_INFO(Kernel_SVC, "Process {} exiting", current_process->GetProcessID());
    ASSERT_MSG(current_process->GetStatus() == ProcessStatus::Running,
               "Process has already exited");

    current_process->PrepareForTermination();

    // Kill the current thread
    system.CurrentScheduler().GetCurrentThread()->Stop();
}

static void ExitProcess32(Core::System& system) {
    ExitProcess(system);
}

/// Creates a new thread
static ResultCode CreateThread(Core::System& system, Handle* out_handle, VAddr entry_point, u64 arg,
                               VAddr stack_top, u32 priority, s32 processor_id) {
    LOG_DEBUG(Kernel_SVC,
              "called entrypoint=0x{:08X}, arg=0x{:08X}, stacktop=0x{:08X}, "
              "threadpriority=0x{:08X}, processorid=0x{:08X} : created handle=0x{:08X}",
              entry_point, arg, stack_top, priority, processor_id, *out_handle);

    auto* const current_process = system.Kernel().CurrentProcess();

    if (processor_id == THREADPROCESSORID_IDEAL) {
        // Set the target CPU to the one specified by the process.
        processor_id = current_process->GetIdealCore();
        ASSERT(processor_id != THREADPROCESSORID_IDEAL);
    }

    if (processor_id < THREADPROCESSORID_0 || processor_id > THREADPROCESSORID_3) {
        LOG_ERROR(Kernel_SVC, "Invalid thread processor ID: {}", processor_id);
        return ERR_INVALID_PROCESSOR_ID;
    }

    const u64 core_mask = current_process->GetCoreMask();
    if ((core_mask | (1ULL << processor_id)) != core_mask) {
        LOG_ERROR(Kernel_SVC, "Invalid thread core specified ({})", processor_id);
        return ERR_INVALID_PROCESSOR_ID;
    }

    if (priority > THREADPRIO_LOWEST) {
        LOG_ERROR(Kernel_SVC,
                  "Invalid thread priority specified ({}). Must be within the range 0-64",
                  priority);
        return ERR_INVALID_THREAD_PRIORITY;
    }

    if (((1ULL << priority) & current_process->GetPriorityMask()) == 0) {
        LOG_ERROR(Kernel_SVC, "Invalid thread priority specified ({})", priority);
        return ERR_INVALID_THREAD_PRIORITY;
    }

    auto& kernel = system.Kernel();

    ASSERT(kernel.CurrentProcess()->GetResourceLimit()->Reserve(ResourceType::Threads, 1));

    ThreadType type = THREADTYPE_USER;
    CASCADE_RESULT(std::shared_ptr<Thread> thread,
                   Thread::Create(system, type, "", entry_point, priority, arg, processor_id,
                                  stack_top, current_process));

    const auto new_thread_handle = current_process->GetHandleTable().Create(thread);
    if (new_thread_handle.Failed()) {
        LOG_ERROR(Kernel_SVC, "Failed to create handle with error=0x{:X}",
                  new_thread_handle.Code().raw);
        return new_thread_handle.Code();
    }
    *out_handle = *new_thread_handle;

    // Set the thread name for debugging purposes.
    thread->SetName(
        fmt::format("thread[entry_point={:X}, handle={:X}]", entry_point, *new_thread_handle));

    return RESULT_SUCCESS;
}

static ResultCode CreateThread32(Core::System& system, Handle* out_handle, u32 priority,
                                 u32 entry_point, u32 arg, u32 stack_top, s32 processor_id) {
    return CreateThread(system, out_handle, static_cast<VAddr>(entry_point), static_cast<u64>(arg),
                        static_cast<VAddr>(stack_top), priority, processor_id);
}

/// Starts the thread for the provided handle
static ResultCode StartThread(Core::System& system, Handle thread_handle) {
    LOG_DEBUG(Kernel_SVC, "called thread=0x{:08X}", thread_handle);

    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    const std::shared_ptr<Thread> thread = handle_table.Get<Thread>(thread_handle);
    if (!thread) {
        LOG_ERROR(Kernel_SVC, "Thread handle does not exist, thread_handle=0x{:08X}",
                  thread_handle);
        return ERR_INVALID_HANDLE;
    }

    ASSERT(thread->GetStatus() == ThreadStatus::Dormant);

    return thread->Start();
}

static ResultCode StartThread32(Core::System& system, Handle thread_handle) {
    return StartThread(system, thread_handle);
}

/// Called when a thread exits
static void ExitThread(Core::System& system) {
    LOG_DEBUG(Kernel_SVC, "called, pc=0x{:08X}", system.CurrentArmInterface().GetPC());

    auto* const current_thread = system.CurrentScheduler().GetCurrentThread();
    system.GlobalScheduler().RemoveThread(SharedFrom(current_thread));
    current_thread->Stop();
}

static void ExitThread32(Core::System& system) {
    ExitThread(system);
}

/// Sleep the current thread
static void SleepThread(Core::System& system, s64 nanoseconds) {
    LOG_DEBUG(Kernel_SVC, "called nanoseconds={}", nanoseconds);

    enum class SleepType : s64 {
        YieldWithoutLoadBalancing = 0,
        YieldWithLoadBalancing = -1,
        YieldAndWaitForLoadBalancing = -2,
    };

    auto& scheduler = system.CurrentScheduler();
    auto* const current_thread = scheduler.GetCurrentThread();
    bool is_redundant = false;

    if (nanoseconds <= 0) {
        switch (static_cast<SleepType>(nanoseconds)) {
        case SleepType::YieldWithoutLoadBalancing: {
            auto pair = current_thread->YieldSimple();
            is_redundant = pair.second;
            break;
        }
        case SleepType::YieldWithLoadBalancing: {
            auto pair = current_thread->YieldAndBalanceLoad();
            is_redundant = pair.second;
            break;
        }
        case SleepType::YieldAndWaitForLoadBalancing: {
            auto pair = current_thread->YieldAndWaitForLoadBalancing();
            is_redundant = pair.second;
            break;
        }
        default:
            UNREACHABLE_MSG("Unimplemented sleep yield type '{:016X}'!", nanoseconds);
        }
    } else {
        current_thread->Sleep(nanoseconds);
    }

    if (is_redundant && !system.Kernel().IsMulticore()) {
        system.Kernel().ExitSVCProfile();
        system.CoreTiming().AddTicks(1000U);
        system.GetCpuManager().PreemptSingleCore();
        system.Kernel().EnterSVCProfile();
    }
}

static void SleepThread32(Core::System& system, u32 nanoseconds_low, u32 nanoseconds_high) {
    const s64 nanoseconds = static_cast<s64>(static_cast<u64>(nanoseconds_low) |
                                             (static_cast<u64>(nanoseconds_high) << 32));
    SleepThread(system, nanoseconds);
}

/// Wait process wide key atomic
static ResultCode WaitProcessWideKeyAtomic(Core::System& system, VAddr mutex_addr,
                                           VAddr condition_variable_addr, Handle thread_handle,
                                           s64 nano_seconds) {
    LOG_TRACE(
        Kernel_SVC,
        "called mutex_addr={:X}, condition_variable_addr={:X}, thread_handle=0x{:08X}, timeout={}",
        mutex_addr, condition_variable_addr, thread_handle, nano_seconds);

    if (Core::Memory::IsKernelVirtualAddress(mutex_addr)) {
        LOG_ERROR(
            Kernel_SVC,
            "Given mutex address must not be within the kernel address space. address=0x{:016X}",
            mutex_addr);
        return ERR_INVALID_ADDRESS_STATE;
    }

    if (!Common::IsWordAligned(mutex_addr)) {
        LOG_ERROR(Kernel_SVC, "Given mutex address must be word-aligned. address=0x{:016X}",
                  mutex_addr);
        return ERR_INVALID_ADDRESS;
    }

    ASSERT(condition_variable_addr == Common::AlignDown(condition_variable_addr, 4));
    auto& kernel = system.Kernel();
    Handle event_handle;
    Thread* current_thread = system.CurrentScheduler().GetCurrentThread();
    auto* const current_process = system.Kernel().CurrentProcess();
    {
        SchedulerLockAndSleep lock(kernel, event_handle, current_thread, nano_seconds);
        const auto& handle_table = current_process->GetHandleTable();
        std::shared_ptr<Thread> thread = handle_table.Get<Thread>(thread_handle);
        ASSERT(thread);

        current_thread->SetSynchronizationResults(nullptr, RESULT_TIMEOUT);

        if (thread->IsPendingTermination()) {
            lock.CancelSleep();
            return ERR_THREAD_TERMINATING;
        }

        const auto release_result = current_process->GetMutex().Release(mutex_addr);
        if (release_result.IsError()) {
            lock.CancelSleep();
            return release_result;
        }

        if (nano_seconds == 0) {
            lock.CancelSleep();
            return RESULT_TIMEOUT;
        }

        current_thread->SetCondVarWaitAddress(condition_variable_addr);
        current_thread->SetMutexWaitAddress(mutex_addr);
        current_thread->SetWaitHandle(thread_handle);
        current_thread->SetStatus(ThreadStatus::WaitCondVar);
        current_process->InsertConditionVariableThread(SharedFrom(current_thread));
    }

    if (event_handle != InvalidHandle) {
        auto& time_manager = kernel.TimeManager();
        time_manager.UnscheduleTimeEvent(event_handle);
    }

    {
        SchedulerLock lock(kernel);

        auto* owner = current_thread->GetLockOwner();
        if (owner != nullptr) {
            owner->RemoveMutexWaiter(SharedFrom(current_thread));
        }

        current_process->RemoveConditionVariableThread(SharedFrom(current_thread));
    }
    // Note: Deliberately don't attempt to inherit the lock owner's priority.

    return current_thread->GetSignalingResult();
}

static ResultCode WaitProcessWideKeyAtomic32(Core::System& system, u32 mutex_addr,
                                             u32 condition_variable_addr, Handle thread_handle,
                                             u32 nanoseconds_low, u32 nanoseconds_high) {
    const s64 nanoseconds =
        static_cast<s64>(nanoseconds_low | (static_cast<u64>(nanoseconds_high) << 32));
    return WaitProcessWideKeyAtomic(system, static_cast<VAddr>(mutex_addr),
                                    static_cast<VAddr>(condition_variable_addr), thread_handle,
                                    nanoseconds);
}

/// Signal process wide key
static void SignalProcessWideKey(Core::System& system, VAddr condition_variable_addr, s32 target) {
    LOG_TRACE(Kernel_SVC, "called, condition_variable_addr=0x{:X}, target=0x{:08X}",
              condition_variable_addr, target);

    ASSERT(condition_variable_addr == Common::AlignDown(condition_variable_addr, 4));

    // Retrieve a list of all threads that are waiting for this condition variable.
    auto& kernel = system.Kernel();
    SchedulerLock lock(kernel);
    auto* const current_process = kernel.CurrentProcess();
    std::vector<std::shared_ptr<Thread>> waiting_threads =
        current_process->GetConditionVariableThreads(condition_variable_addr);

    // Only process up to 'target' threads, unless 'target' is less equal 0, in which case process
    // them all.
    std::size_t last = waiting_threads.size();
    if (target > 0) {
        last = std::min(waiting_threads.size(), static_cast<std::size_t>(target));
    }
    for (std::size_t index = 0; index < last; ++index) {
        auto& thread = waiting_threads[index];

        ASSERT(thread->GetCondVarWaitAddress() == condition_variable_addr);

        // liberate Cond Var Thread.
        current_process->RemoveConditionVariableThread(thread);

        const std::size_t current_core = system.CurrentCoreIndex();
        auto& monitor = system.Monitor();

        // Atomically read the value of the mutex.
        u32 mutex_val = 0;
        u32 update_val = 0;
        const VAddr mutex_address = thread->GetMutexWaitAddress();
        do {
            // If the mutex is not yet acquired, acquire it.
            mutex_val = monitor.ExclusiveRead32(current_core, mutex_address);

            if (mutex_val != 0) {
                update_val = mutex_val | Mutex::MutexHasWaitersFlag;
            } else {
                update_val = thread->GetWaitHandle();
            }
        } while (!monitor.ExclusiveWrite32(current_core, mutex_address, update_val));
        monitor.ClearExclusive();
        if (mutex_val == 0) {
            // We were able to acquire the mutex, resume this thread.
            auto* const lock_owner = thread->GetLockOwner();
            if (lock_owner != nullptr) {
                lock_owner->RemoveMutexWaiter(thread);
            }

            thread->SetLockOwner(nullptr);
            thread->SetSynchronizationResults(nullptr, RESULT_SUCCESS);
            thread->ResumeFromWait();
        } else {
            // The mutex is already owned by some other thread, make this thread wait on it.
            const Handle owner_handle = static_cast<Handle>(mutex_val & Mutex::MutexOwnerMask);
            const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
            auto owner = handle_table.Get<Thread>(owner_handle);
            ASSERT(owner);
            if (thread->GetStatus() == ThreadStatus::WaitCondVar) {
                thread->SetStatus(ThreadStatus::WaitMutex);
            }

            owner->AddMutexWaiter(thread);
        }
    }
}

static void SignalProcessWideKey32(Core::System& system, u32 condition_variable_addr, s32 target) {
    SignalProcessWideKey(system, condition_variable_addr, target);
}

// Wait for an address (via Address Arbiter)
static ResultCode WaitForAddress(Core::System& system, VAddr address, u32 type, s32 value,
                                 s64 timeout) {
    LOG_TRACE(Kernel_SVC, "called, address=0x{:X}, type=0x{:X}, value=0x{:X}, timeout={}", address,
              type, value, timeout);

    // If the passed address is a kernel virtual address, return invalid memory state.
    if (Core::Memory::IsKernelVirtualAddress(address)) {
        LOG_ERROR(Kernel_SVC, "Address is a kernel virtual address, address={:016X}", address);
        return ERR_INVALID_ADDRESS_STATE;
    }

    // If the address is not properly aligned to 4 bytes, return invalid address.
    if (!Common::IsWordAligned(address)) {
        LOG_ERROR(Kernel_SVC, "Address is not word aligned, address={:016X}", address);
        return ERR_INVALID_ADDRESS;
    }

    const auto arbitration_type = static_cast<AddressArbiter::ArbitrationType>(type);
    auto& address_arbiter = system.Kernel().CurrentProcess()->GetAddressArbiter();
    const ResultCode result =
        address_arbiter.WaitForAddress(address, arbitration_type, value, timeout);
    return result;
}

static ResultCode WaitForAddress32(Core::System& system, u32 address, u32 type, s32 value,
                                   u32 timeout_low, u32 timeout_high) {
    s64 timeout = static_cast<s64>(timeout_low | (static_cast<u64>(timeout_high) << 32));
    return WaitForAddress(system, static_cast<VAddr>(address), type, value, timeout);
}

// Signals to an address (via Address Arbiter)
static ResultCode SignalToAddress(Core::System& system, VAddr address, u32 type, s32 value,
                                  s32 num_to_wake) {
    LOG_TRACE(Kernel_SVC, "called, address=0x{:X}, type=0x{:X}, value=0x{:X}, num_to_wake=0x{:X}",
              address, type, value, num_to_wake);

    // If the passed address is a kernel virtual address, return invalid memory state.
    if (Core::Memory::IsKernelVirtualAddress(address)) {
        LOG_ERROR(Kernel_SVC, "Address is a kernel virtual address, address={:016X}", address);
        return ERR_INVALID_ADDRESS_STATE;
    }

    // If the address is not properly aligned to 4 bytes, return invalid address.
    if (!Common::IsWordAligned(address)) {
        LOG_ERROR(Kernel_SVC, "Address is not word aligned, address={:016X}", address);
        return ERR_INVALID_ADDRESS;
    }

    const auto signal_type = static_cast<AddressArbiter::SignalType>(type);
    auto& address_arbiter = system.Kernel().CurrentProcess()->GetAddressArbiter();
    return address_arbiter.SignalToAddress(address, signal_type, value, num_to_wake);
}

static ResultCode SignalToAddress32(Core::System& system, u32 address, u32 type, s32 value,
                                    s32 num_to_wake) {
    return SignalToAddress(system, static_cast<VAddr>(address), type, value, num_to_wake);
}

static void KernelDebug([[maybe_unused]] Core::System& system,
                        [[maybe_unused]] u32 kernel_debug_type, [[maybe_unused]] u64 param1,
                        [[maybe_unused]] u64 param2, [[maybe_unused]] u64 param3) {
    // Intentionally do nothing, as this does nothing in released kernel binaries.
}

static void ChangeKernelTraceState([[maybe_unused]] Core::System& system,
                                   [[maybe_unused]] u32 trace_state) {
    // Intentionally do nothing, as this does nothing in released kernel binaries.
}

/// This returns the total CPU ticks elapsed since the CPU was powered-on
static u64 GetSystemTick(Core::System& system) {
    LOG_TRACE(Kernel_SVC, "called");

    auto& core_timing = system.CoreTiming();

    // Returns the value of cntpct_el0 (https://switchbrew.org/wiki/SVC#svcGetSystemTick)
    const u64 result{system.CoreTiming().GetClockTicks()};

    if (!system.Kernel().IsMulticore()) {
        core_timing.AddTicks(400U);
    }

    return result;
}

static void GetSystemTick32(Core::System& system, u32* time_low, u32* time_high) {
    u64 time = GetSystemTick(system);
    *time_low = static_cast<u32>(time);
    *time_high = static_cast<u32>(time >> 32);
}

/// Close a handle
static ResultCode CloseHandle(Core::System& system, Handle handle) {
    LOG_TRACE(Kernel_SVC, "Closing handle 0x{:08X}", handle);

    auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    return handle_table.Close(handle);
}

static ResultCode CloseHandle32(Core::System& system, Handle handle) {
    return CloseHandle(system, handle);
}

/// Clears the signaled state of an event or process.
static ResultCode ResetSignal(Core::System& system, Handle handle) {
    LOG_DEBUG(Kernel_SVC, "called handle 0x{:08X}", handle);

    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();

    auto event = handle_table.Get<ReadableEvent>(handle);
    if (event) {
        return event->Reset();
    }

    auto process = handle_table.Get<Process>(handle);
    if (process) {
        return process->ClearSignalState();
    }

    LOG_ERROR(Kernel_SVC, "Invalid handle (0x{:08X})", handle);
    return ERR_INVALID_HANDLE;
}

static ResultCode ResetSignal32(Core::System& system, Handle handle) {
    return ResetSignal(system, handle);
}

/// Creates a TransferMemory object
static ResultCode CreateTransferMemory(Core::System& system, Handle* handle, VAddr addr, u64 size,
                                       u32 permissions) {
    std::lock_guard lock{HLE::g_hle_lock};
    LOG_DEBUG(Kernel_SVC, "called addr=0x{:X}, size=0x{:X}, perms=0x{:08X}", addr, size,
              permissions);

    if (!Common::Is4KBAligned(addr)) {
        LOG_ERROR(Kernel_SVC, "Address ({:016X}) is not page aligned!", addr);
        return ERR_INVALID_ADDRESS;
    }

    if (!Common::Is4KBAligned(size) || size == 0) {
        LOG_ERROR(Kernel_SVC, "Size ({:016X}) is not page aligned or equal to zero!", size);
        return ERR_INVALID_ADDRESS;
    }

    if (!IsValidAddressRange(addr, size)) {
        LOG_ERROR(Kernel_SVC, "Address and size cause overflow! (address={:016X}, size={:016X})",
                  addr, size);
        return ERR_INVALID_ADDRESS_STATE;
    }

    const auto perms{static_cast<Memory::MemoryPermission>(permissions)};
    if (perms > Memory::MemoryPermission::ReadAndWrite ||
        perms == Memory::MemoryPermission::Write) {
        LOG_ERROR(Kernel_SVC, "Invalid memory permissions for transfer memory! (perms={:08X})",
                  permissions);
        return ERR_INVALID_MEMORY_PERMISSIONS;
    }

    auto& kernel = system.Kernel();
    auto transfer_mem_handle = TransferMemory::Create(kernel, system.Memory(), addr, size, perms);

    if (const auto reserve_result{transfer_mem_handle->Reserve()}; reserve_result.IsError()) {
        return reserve_result;
    }

    auto& handle_table = kernel.CurrentProcess()->GetHandleTable();
    const auto result{handle_table.Create(std::move(transfer_mem_handle))};
    if (result.Failed()) {
        return result.Code();
    }

    *handle = *result;
    return RESULT_SUCCESS;
}

static ResultCode CreateTransferMemory32(Core::System& system, Handle* handle, u32 addr, u32 size,
                                         u32 permissions) {
    return CreateTransferMemory(system, handle, static_cast<VAddr>(addr),
                                static_cast<std::size_t>(size), permissions);
}

static ResultCode GetThreadCoreMask(Core::System& system, Handle thread_handle, u32* core,
                                    u64* mask) {
    LOG_TRACE(Kernel_SVC, "called, handle=0x{:08X}", thread_handle);

    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    const std::shared_ptr<Thread> thread = handle_table.Get<Thread>(thread_handle);
    if (!thread) {
        LOG_ERROR(Kernel_SVC, "Thread handle does not exist, thread_handle=0x{:08X}",
                  thread_handle);
        *core = 0;
        *mask = 0;
        return ERR_INVALID_HANDLE;
    }

    *core = thread->GetIdealCore();
    *mask = thread->GetAffinityMask();

    return RESULT_SUCCESS;
}

static ResultCode GetThreadCoreMask32(Core::System& system, Handle thread_handle, u32* core,
                                      u32* mask_low, u32* mask_high) {
    u64 mask{};
    const auto result = GetThreadCoreMask(system, thread_handle, core, &mask);
    *mask_high = static_cast<u32>(mask >> 32);
    *mask_low = static_cast<u32>(mask);
    return result;
}

static ResultCode SetThreadCoreMask(Core::System& system, Handle thread_handle, u32 core,
                                    u64 affinity_mask) {
    LOG_DEBUG(Kernel_SVC, "called, handle=0x{:08X}, core=0x{:X}, affinity_mask=0x{:016X}",
              thread_handle, core, affinity_mask);

    const auto* const current_process = system.Kernel().CurrentProcess();

    if (core == static_cast<u32>(THREADPROCESSORID_IDEAL)) {
        const u8 ideal_cpu_core = current_process->GetIdealCore();

        ASSERT(ideal_cpu_core != static_cast<u8>(THREADPROCESSORID_IDEAL));

        // Set the target CPU to the ideal core specified by the process.
        core = ideal_cpu_core;
        affinity_mask = 1ULL << core;
    } else {
        const u64 core_mask = current_process->GetCoreMask();

        if ((core_mask | affinity_mask) != core_mask) {
            LOG_ERROR(
                Kernel_SVC,
                "Invalid processor ID specified (core_mask=0x{:08X}, affinity_mask=0x{:016X})",
                core_mask, affinity_mask);
            return ERR_INVALID_PROCESSOR_ID;
        }

        if (affinity_mask == 0) {
            LOG_ERROR(Kernel_SVC, "Specfified affinity mask is zero.");
            return ERR_INVALID_COMBINATION;
        }

        if (core < Core::Hardware::NUM_CPU_CORES) {
            if ((affinity_mask & (1ULL << core)) == 0) {
                LOG_ERROR(Kernel_SVC,
                          "Core is not enabled for the current mask, core={}, mask={:016X}", core,
                          affinity_mask);
                return ERR_INVALID_COMBINATION;
            }
        } else if (core != static_cast<u32>(THREADPROCESSORID_DONT_CARE) &&
                   core != static_cast<u32>(THREADPROCESSORID_DONT_UPDATE)) {
            LOG_ERROR(Kernel_SVC, "Invalid processor ID specified (core={}).", core);
            return ERR_INVALID_PROCESSOR_ID;
        }
    }

    const auto& handle_table = current_process->GetHandleTable();
    const std::shared_ptr<Thread> thread = handle_table.Get<Thread>(thread_handle);
    if (!thread) {
        LOG_ERROR(Kernel_SVC, "Thread handle does not exist, thread_handle=0x{:08X}",
                  thread_handle);
        return ERR_INVALID_HANDLE;
    }

    return thread->SetCoreAndAffinityMask(core, affinity_mask);
}

static ResultCode SetThreadCoreMask32(Core::System& system, Handle thread_handle, u32 core,
                                      u32 affinity_mask_low, u32 affinity_mask_high) {
    const u64 affinity_mask =
        static_cast<u64>(affinity_mask_low) | (static_cast<u64>(affinity_mask_high) << 32);
    return SetThreadCoreMask(system, thread_handle, core, affinity_mask);
}

static ResultCode CreateEvent(Core::System& system, Handle* write_handle, Handle* read_handle) {
    LOG_DEBUG(Kernel_SVC, "called");

    auto& kernel = system.Kernel();
    const auto [readable_event, writable_event] =
        WritableEvent::CreateEventPair(kernel, "CreateEvent");

    HandleTable& handle_table = kernel.CurrentProcess()->GetHandleTable();

    const auto write_create_result = handle_table.Create(writable_event);
    if (write_create_result.Failed()) {
        return write_create_result.Code();
    }
    *write_handle = *write_create_result;

    const auto read_create_result = handle_table.Create(readable_event);
    if (read_create_result.Failed()) {
        handle_table.Close(*write_create_result);
        return read_create_result.Code();
    }
    *read_handle = *read_create_result;

    LOG_DEBUG(Kernel_SVC,
              "successful. Writable event handle=0x{:08X}, Readable event handle=0x{:08X}",
              *write_create_result, *read_create_result);
    return RESULT_SUCCESS;
}

static ResultCode CreateEvent32(Core::System& system, Handle* write_handle, Handle* read_handle) {
    return CreateEvent(system, write_handle, read_handle);
}

static ResultCode ClearEvent(Core::System& system, Handle handle) {
    LOG_TRACE(Kernel_SVC, "called, event=0x{:08X}", handle);

    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();

    auto writable_event = handle_table.Get<WritableEvent>(handle);
    if (writable_event) {
        writable_event->Clear();
        return RESULT_SUCCESS;
    }

    auto readable_event = handle_table.Get<ReadableEvent>(handle);
    if (readable_event) {
        readable_event->Clear();
        return RESULT_SUCCESS;
    }

    LOG_ERROR(Kernel_SVC, "Event handle does not exist, handle=0x{:08X}", handle);
    return ERR_INVALID_HANDLE;
}

static ResultCode ClearEvent32(Core::System& system, Handle handle) {
    return ClearEvent(system, handle);
}

static ResultCode SignalEvent(Core::System& system, Handle handle) {
    LOG_DEBUG(Kernel_SVC, "called. Handle=0x{:08X}", handle);

    HandleTable& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    auto writable_event = handle_table.Get<WritableEvent>(handle);

    if (!writable_event) {
        LOG_ERROR(Kernel_SVC, "Non-existent writable event handle used (0x{:08X})", handle);
        return ERR_INVALID_HANDLE;
    }

    writable_event->Signal();
    return RESULT_SUCCESS;
}

static ResultCode SignalEvent32(Core::System& system, Handle handle) {
    return SignalEvent(system, handle);
}

static ResultCode GetProcessInfo(Core::System& system, u64* out, Handle process_handle, u32 type) {
    LOG_DEBUG(Kernel_SVC, "called, handle=0x{:08X}, type=0x{:X}", process_handle, type);

    // This function currently only allows retrieving a process' status.
    enum class InfoType {
        Status,
    };

    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    const auto process = handle_table.Get<Process>(process_handle);
    if (!process) {
        LOG_ERROR(Kernel_SVC, "Process handle does not exist, process_handle=0x{:08X}",
                  process_handle);
        return ERR_INVALID_HANDLE;
    }

    const auto info_type = static_cast<InfoType>(type);
    if (info_type != InfoType::Status) {
        LOG_ERROR(Kernel_SVC, "Expected info_type to be Status but got {} instead", type);
        return ERR_INVALID_ENUM_VALUE;
    }

    *out = static_cast<u64>(process->GetStatus());
    return RESULT_SUCCESS;
}

static ResultCode CreateResourceLimit(Core::System& system, Handle* out_handle) {
    std::lock_guard lock{HLE::g_hle_lock};
    LOG_DEBUG(Kernel_SVC, "called");

    auto& kernel = system.Kernel();
    auto resource_limit = ResourceLimit::Create(kernel);

    auto* const current_process = kernel.CurrentProcess();
    ASSERT(current_process != nullptr);

    const auto handle = current_process->GetHandleTable().Create(std::move(resource_limit));
    if (handle.Failed()) {
        return handle.Code();
    }

    *out_handle = *handle;
    return RESULT_SUCCESS;
}

static ResultCode GetResourceLimitLimitValue(Core::System& system, u64* out_value,
                                             Handle resource_limit, u32 resource_type) {
    LOG_DEBUG(Kernel_SVC, "called. Handle={:08X}, Resource type={}", resource_limit, resource_type);

    const auto limit_value = RetrieveResourceLimitValue(system, resource_limit, resource_type,
                                                        ResourceLimitValueType::LimitValue);
    if (limit_value.Failed()) {
        return limit_value.Code();
    }

    *out_value = static_cast<u64>(*limit_value);
    return RESULT_SUCCESS;
}

static ResultCode GetResourceLimitCurrentValue(Core::System& system, u64* out_value,
                                               Handle resource_limit, u32 resource_type) {
    LOG_DEBUG(Kernel_SVC, "called. Handle={:08X}, Resource type={}", resource_limit, resource_type);

    const auto current_value = RetrieveResourceLimitValue(system, resource_limit, resource_type,
                                                          ResourceLimitValueType::CurrentValue);
    if (current_value.Failed()) {
        return current_value.Code();
    }

    *out_value = static_cast<u64>(*current_value);
    return RESULT_SUCCESS;
}

static ResultCode SetResourceLimitLimitValue(Core::System& system, Handle resource_limit,
                                             u32 resource_type, u64 value) {
    LOG_DEBUG(Kernel_SVC, "called. Handle={:08X}, Resource type={}, Value={}", resource_limit,
              resource_type, value);

    const auto type = static_cast<ResourceType>(resource_type);
    if (!IsValidResourceType(type)) {
        LOG_ERROR(Kernel_SVC, "Invalid resource limit type: '{}'", resource_type);
        return ERR_INVALID_ENUM_VALUE;
    }

    auto* const current_process = system.Kernel().CurrentProcess();
    ASSERT(current_process != nullptr);

    auto resource_limit_object =
        current_process->GetHandleTable().Get<ResourceLimit>(resource_limit);
    if (!resource_limit_object) {
        LOG_ERROR(Kernel_SVC, "Handle to non-existent resource limit instance used. Handle={:08X}",
                  resource_limit);
        return ERR_INVALID_HANDLE;
    }

    const auto set_result = resource_limit_object->SetLimitValue(type, static_cast<s64>(value));
    if (set_result.IsError()) {
        LOG_ERROR(
            Kernel_SVC,
            "Attempted to lower resource limit ({}) for category '{}' below its current value ({})",
            resource_limit_object->GetMaxResourceValue(type), resource_type,
            resource_limit_object->GetCurrentResourceValue(type));
        return set_result;
    }

    return RESULT_SUCCESS;
}

static ResultCode GetProcessList(Core::System& system, u32* out_num_processes,
                                 VAddr out_process_ids, u32 out_process_ids_size) {
    LOG_DEBUG(Kernel_SVC, "called. out_process_ids=0x{:016X}, out_process_ids_size={}",
              out_process_ids, out_process_ids_size);

    // If the supplied size is negative or greater than INT32_MAX / sizeof(u64), bail.
    if ((out_process_ids_size & 0xF0000000) != 0) {
        LOG_ERROR(Kernel_SVC,
                  "Supplied size outside [0, 0x0FFFFFFF] range. out_process_ids_size={}",
                  out_process_ids_size);
        return ERR_OUT_OF_RANGE;
    }

    const auto& kernel = system.Kernel();
    const auto total_copy_size = out_process_ids_size * sizeof(u64);

    if (out_process_ids_size > 0 && !kernel.CurrentProcess()->PageTable().IsInsideAddressSpace(
                                        out_process_ids, total_copy_size)) {
        LOG_ERROR(Kernel_SVC, "Address range outside address space. begin=0x{:016X}, end=0x{:016X}",
                  out_process_ids, out_process_ids + total_copy_size);
        return ERR_INVALID_ADDRESS_STATE;
    }

    auto& memory = system.Memory();
    const auto& process_list = kernel.GetProcessList();
    const auto num_processes = process_list.size();
    const auto copy_amount = std::min(std::size_t{out_process_ids_size}, num_processes);

    for (std::size_t i = 0; i < copy_amount; ++i) {
        memory.Write64(out_process_ids, process_list[i]->GetProcessID());
        out_process_ids += sizeof(u64);
    }

    *out_num_processes = static_cast<u32>(num_processes);
    return RESULT_SUCCESS;
}

static ResultCode GetThreadList(Core::System& system, u32* out_num_threads, VAddr out_thread_ids,
                                u32 out_thread_ids_size, Handle debug_handle) {
    // TODO: Handle this case when debug events are supported.
    UNIMPLEMENTED_IF(debug_handle != InvalidHandle);

    LOG_DEBUG(Kernel_SVC, "called. out_thread_ids=0x{:016X}, out_thread_ids_size={}",
              out_thread_ids, out_thread_ids_size);

    // If the size is negative or larger than INT32_MAX / sizeof(u64)
    if ((out_thread_ids_size & 0xF0000000) != 0) {
        LOG_ERROR(Kernel_SVC, "Supplied size outside [0, 0x0FFFFFFF] range. size={}",
                  out_thread_ids_size);
        return ERR_OUT_OF_RANGE;
    }

    const auto* const current_process = system.Kernel().CurrentProcess();
    const auto total_copy_size = out_thread_ids_size * sizeof(u64);

    if (out_thread_ids_size > 0 &&
        !current_process->PageTable().IsInsideAddressSpace(out_thread_ids, total_copy_size)) {
        LOG_ERROR(Kernel_SVC, "Address range outside address space. begin=0x{:016X}, end=0x{:016X}",
                  out_thread_ids, out_thread_ids + total_copy_size);
        return ERR_INVALID_ADDRESS_STATE;
    }

    auto& memory = system.Memory();
    const auto& thread_list = current_process->GetThreadList();
    const auto num_threads = thread_list.size();
    const auto copy_amount = std::min(std::size_t{out_thread_ids_size}, num_threads);

    auto list_iter = thread_list.cbegin();
    for (std::size_t i = 0; i < copy_amount; ++i, ++list_iter) {
        memory.Write64(out_thread_ids, (*list_iter)->GetThreadID());
        out_thread_ids += sizeof(u64);
    }

    *out_num_threads = static_cast<u32>(num_threads);
    return RESULT_SUCCESS;
}

static ResultCode FlushProcessDataCache32(Core::System& system, Handle handle, u32 address,
                                          u32 size) {
    // Note(Blinkhawk): For emulation purposes of the data cache this is mostly a nope
    // as all emulation is done in the same cache level in host architecture, thus data cache
    // does not need flushing.
    LOG_DEBUG(Kernel_SVC, "called");
    return RESULT_SUCCESS;
}

namespace {
struct FunctionDef {
    using Func = void(Core::System&);

    u32 id;
    Func* func;
    const char* name;
};
} // namespace

static const FunctionDef SVC_Table_32[] = {
    {0x00, nullptr, "Unknown"},
    {0x01, SvcWrap32<SetHeapSize32>, "SetHeapSize32"},
    {0x02, nullptr, "Unknown"},
    {0x03, SvcWrap32<SetMemoryAttribute32>, "SetMemoryAttribute32"},
    {0x04, SvcWrap32<MapMemory32>, "MapMemory32"},
    {0x05, SvcWrap32<UnmapMemory32>, "UnmapMemory32"},
    {0x06, SvcWrap32<QueryMemory32>, "QueryMemory32"},
    {0x07, SvcWrap32<ExitProcess32>, "ExitProcess32"},
    {0x08, SvcWrap32<CreateThread32>, "CreateThread32"},
    {0x09, SvcWrap32<StartThread32>, "StartThread32"},
    {0x0a, SvcWrap32<ExitThread32>, "ExitThread32"},
    {0x0b, SvcWrap32<SleepThread32>, "SleepThread32"},
    {0x0c, SvcWrap32<GetThreadPriority32>, "GetThreadPriority32"},
    {0x0d, SvcWrap32<SetThreadPriority32>, "SetThreadPriority32"},
    {0x0e, SvcWrap32<GetThreadCoreMask32>, "GetThreadCoreMask32"},
    {0x0f, SvcWrap32<SetThreadCoreMask32>, "SetThreadCoreMask32"},
    {0x10, SvcWrap32<GetCurrentProcessorNumber32>, "GetCurrentProcessorNumber32"},
    {0x11, SvcWrap32<SignalEvent32>, "SignalEvent32"},
    {0x12, SvcWrap32<ClearEvent32>, "ClearEvent32"},
    {0x13, SvcWrap32<MapSharedMemory32>, "MapSharedMemory32"},
    {0x14, nullptr, "UnmapSharedMemory32"},
    {0x15, SvcWrap32<CreateTransferMemory32>, "CreateTransferMemory32"},
    {0x16, SvcWrap32<CloseHandle32>, "CloseHandle32"},
    {0x17, SvcWrap32<ResetSignal32>, "ResetSignal32"},
    {0x18, SvcWrap32<WaitSynchronization32>, "WaitSynchronization32"},
    {0x19, SvcWrap32<CancelSynchronization32>, "CancelSynchronization32"},
    {0x1a, SvcWrap32<ArbitrateLock32>, "ArbitrateLock32"},
    {0x1b, SvcWrap32<ArbitrateUnlock32>, "ArbitrateUnlock32"},
    {0x1c, SvcWrap32<WaitProcessWideKeyAtomic32>, "WaitProcessWideKeyAtomic32"},
    {0x1d, SvcWrap32<SignalProcessWideKey32>, "SignalProcessWideKey32"},
    {0x1e, SvcWrap32<GetSystemTick32>, "GetSystemTick32"},
    {0x1f, SvcWrap32<ConnectToNamedPort32>, "ConnectToNamedPort32"},
    {0x20, nullptr, "Unknown"},
    {0x21, SvcWrap32<SendSyncRequest32>, "SendSyncRequest32"},
    {0x22, nullptr, "SendSyncRequestWithUserBuffer32"},
    {0x23, nullptr, "Unknown"},
    {0x24, SvcWrap32<GetProcessId32>, "GetProcessId32"},
    {0x25, SvcWrap32<GetThreadId32>, "GetThreadId32"},
    {0x26, SvcWrap32<Break32>, "Break32"},
    {0x27, nullptr, "OutputDebugString32"},
    {0x28, nullptr, "Unknown"},
    {0x29, SvcWrap32<GetInfo32>, "GetInfo32"},
    {0x2a, nullptr, "Unknown"},
    {0x2b, nullptr, "Unknown"},
    {0x2c, SvcWrap32<MapPhysicalMemory32>, "MapPhysicalMemory32"},
    {0x2d, SvcWrap32<UnmapPhysicalMemory32>, "UnmapPhysicalMemory32"},
    {0x2e, nullptr, "Unknown"},
    {0x2f, nullptr, "Unknown"},
    {0x30, nullptr, "Unknown"},
    {0x31, nullptr, "Unknown"},
    {0x32, SvcWrap32<SetThreadActivity32>, "SetThreadActivity32"},
    {0x33, SvcWrap32<GetThreadContext32>, "GetThreadContext32"},
    {0x34, SvcWrap32<WaitForAddress32>, "WaitForAddress32"},
    {0x35, SvcWrap32<SignalToAddress32>, "SignalToAddress32"},
    {0x36, nullptr, "Unknown"},
    {0x37, nullptr, "Unknown"},
    {0x38, nullptr, "Unknown"},
    {0x39, nullptr, "Unknown"},
    {0x3a, nullptr, "Unknown"},
    {0x3b, nullptr, "Unknown"},
    {0x3c, nullptr, "Unknown"},
    {0x3d, nullptr, "Unknown"},
    {0x3e, nullptr, "Unknown"},
    {0x3f, nullptr, "Unknown"},
    {0x40, nullptr, "CreateSession32"},
    {0x41, nullptr, "AcceptSession32"},
    {0x42, nullptr, "Unknown"},
    {0x43, nullptr, "ReplyAndReceive32"},
    {0x44, nullptr, "Unknown"},
    {0x45, SvcWrap32<CreateEvent32>, "CreateEvent32"},
    {0x46, nullptr, "Unknown"},
    {0x47, nullptr, "Unknown"},
    {0x48, nullptr, "Unknown"},
    {0x49, nullptr, "Unknown"},
    {0x4a, nullptr, "Unknown"},
    {0x4b, nullptr, "Unknown"},
    {0x4c, nullptr, "Unknown"},
    {0x4d, nullptr, "Unknown"},
    {0x4e, nullptr, "Unknown"},
    {0x4f, nullptr, "Unknown"},
    {0x50, nullptr, "Unknown"},
    {0x51, nullptr, "Unknown"},
    {0x52, nullptr, "Unknown"},
    {0x53, nullptr, "Unknown"},
    {0x54, nullptr, "Unknown"},
    {0x55, nullptr, "Unknown"},
    {0x56, nullptr, "Unknown"},
    {0x57, nullptr, "Unknown"},
    {0x58, nullptr, "Unknown"},
    {0x59, nullptr, "Unknown"},
    {0x5a, nullptr, "Unknown"},
    {0x5b, nullptr, "Unknown"},
    {0x5c, nullptr, "Unknown"},
    {0x5d, nullptr, "Unknown"},
    {0x5e, nullptr, "Unknown"},
    {0x5F, SvcWrap32<FlushProcessDataCache32>, "FlushProcessDataCache32"},
    {0x60, nullptr, "Unknown"},
    {0x61, nullptr, "Unknown"},
    {0x62, nullptr, "Unknown"},
    {0x63, nullptr, "Unknown"},
    {0x64, nullptr, "Unknown"},
    {0x65, nullptr, "GetProcessList32"},
    {0x66, nullptr, "Unknown"},
    {0x67, nullptr, "Unknown"},
    {0x68, nullptr, "Unknown"},
    {0x69, nullptr, "Unknown"},
    {0x6A, nullptr, "Unknown"},
    {0x6B, nullptr, "Unknown"},
    {0x6C, nullptr, "Unknown"},
    {0x6D, nullptr, "Unknown"},
    {0x6E, nullptr, "Unknown"},
    {0x6f, nullptr, "GetSystemInfo32"},
    {0x70, nullptr, "CreatePort32"},
    {0x71, nullptr, "ManageNamedPort32"},
    {0x72, nullptr, "ConnectToPort32"},
    {0x73, nullptr, "SetProcessMemoryPermission32"},
    {0x74, nullptr, "Unknown"},
    {0x75, nullptr, "Unknown"},
    {0x76, nullptr, "Unknown"},
    {0x77, nullptr, "MapProcessCodeMemory32"},
    {0x78, nullptr, "UnmapProcessCodeMemory32"},
    {0x79, nullptr, "Unknown"},
    {0x7A, nullptr, "Unknown"},
    {0x7B, nullptr, "TerminateProcess32"},
};

static const FunctionDef SVC_Table_64[] = {
    {0x00, nullptr, "Unknown"},
    {0x01, SvcWrap64<SetHeapSize>, "SetHeapSize"},
    {0x02, nullptr, "SetMemoryPermission"},
    {0x03, SvcWrap64<SetMemoryAttribute>, "SetMemoryAttribute"},
    {0x04, SvcWrap64<MapMemory>, "MapMemory"},
    {0x05, SvcWrap64<UnmapMemory>, "UnmapMemory"},
    {0x06, SvcWrap64<QueryMemory>, "QueryMemory"},
    {0x07, SvcWrap64<ExitProcess>, "ExitProcess"},
    {0x08, SvcWrap64<CreateThread>, "CreateThread"},
    {0x09, SvcWrap64<StartThread>, "StartThread"},
    {0x0A, SvcWrap64<ExitThread>, "ExitThread"},
    {0x0B, SvcWrap64<SleepThread>, "SleepThread"},
    {0x0C, SvcWrap64<GetThreadPriority>, "GetThreadPriority"},
    {0x0D, SvcWrap64<SetThreadPriority>, "SetThreadPriority"},
    {0x0E, SvcWrap64<GetThreadCoreMask>, "GetThreadCoreMask"},
    {0x0F, SvcWrap64<SetThreadCoreMask>, "SetThreadCoreMask"},
    {0x10, SvcWrap64<GetCurrentProcessorNumber>, "GetCurrentProcessorNumber"},
    {0x11, SvcWrap64<SignalEvent>, "SignalEvent"},
    {0x12, SvcWrap64<ClearEvent>, "ClearEvent"},
    {0x13, SvcWrap64<MapSharedMemory>, "MapSharedMemory"},
    {0x14, nullptr, "UnmapSharedMemory"},
    {0x15, SvcWrap64<CreateTransferMemory>, "CreateTransferMemory"},
    {0x16, SvcWrap64<CloseHandle>, "CloseHandle"},
    {0x17, SvcWrap64<ResetSignal>, "ResetSignal"},
    {0x18, SvcWrap64<WaitSynchronization>, "WaitSynchronization"},
    {0x19, SvcWrap64<CancelSynchronization>, "CancelSynchronization"},
    {0x1A, SvcWrap64<ArbitrateLock>, "ArbitrateLock"},
    {0x1B, SvcWrap64<ArbitrateUnlock>, "ArbitrateUnlock"},
    {0x1C, SvcWrap64<WaitProcessWideKeyAtomic>, "WaitProcessWideKeyAtomic"},
    {0x1D, SvcWrap64<SignalProcessWideKey>, "SignalProcessWideKey"},
    {0x1E, SvcWrap64<GetSystemTick>, "GetSystemTick"},
    {0x1F, SvcWrap64<ConnectToNamedPort>, "ConnectToNamedPort"},
    {0x20, nullptr, "SendSyncRequestLight"},
    {0x21, SvcWrap64<SendSyncRequest>, "SendSyncRequest"},
    {0x22, nullptr, "SendSyncRequestWithUserBuffer"},
    {0x23, nullptr, "SendAsyncRequestWithUserBuffer"},
    {0x24, SvcWrap64<GetProcessId>, "GetProcessId"},
    {0x25, SvcWrap64<GetThreadId>, "GetThreadId"},
    {0x26, SvcWrap64<Break>, "Break"},
    {0x27, SvcWrap64<OutputDebugString>, "OutputDebugString"},
    {0x28, nullptr, "ReturnFromException"},
    {0x29, SvcWrap64<GetInfo>, "GetInfo"},
    {0x2A, nullptr, "FlushEntireDataCache"},
    {0x2B, nullptr, "FlushDataCache"},
    {0x2C, SvcWrap64<MapPhysicalMemory>, "MapPhysicalMemory"},
    {0x2D, SvcWrap64<UnmapPhysicalMemory>, "UnmapPhysicalMemory"},
    {0x2E, nullptr, "GetFutureThreadInfo"},
    {0x2F, nullptr, "GetLastThreadInfo"},
    {0x30, SvcWrap64<GetResourceLimitLimitValue>, "GetResourceLimitLimitValue"},
    {0x31, SvcWrap64<GetResourceLimitCurrentValue>, "GetResourceLimitCurrentValue"},
    {0x32, SvcWrap64<SetThreadActivity>, "SetThreadActivity"},
    {0x33, SvcWrap64<GetThreadContext>, "GetThreadContext"},
    {0x34, SvcWrap64<WaitForAddress>, "WaitForAddress"},
    {0x35, SvcWrap64<SignalToAddress>, "SignalToAddress"},
    {0x36, nullptr, "SynchronizePreemptionState"},
    {0x37, nullptr, "Unknown"},
    {0x38, nullptr, "Unknown"},
    {0x39, nullptr, "Unknown"},
    {0x3A, nullptr, "Unknown"},
    {0x3B, nullptr, "Unknown"},
    {0x3C, SvcWrap64<KernelDebug>, "KernelDebug"},
    {0x3D, SvcWrap64<ChangeKernelTraceState>, "ChangeKernelTraceState"},
    {0x3E, nullptr, "Unknown"},
    {0x3F, nullptr, "Unknown"},
    {0x40, nullptr, "CreateSession"},
    {0x41, nullptr, "AcceptSession"},
    {0x42, nullptr, "ReplyAndReceiveLight"},
    {0x43, nullptr, "ReplyAndReceive"},
    {0x44, nullptr, "ReplyAndReceiveWithUserBuffer"},
    {0x45, SvcWrap64<CreateEvent>, "CreateEvent"},
    {0x46, nullptr, "Unknown"},
    {0x47, nullptr, "Unknown"},
    {0x48, nullptr, "MapPhysicalMemoryUnsafe"},
    {0x49, nullptr, "UnmapPhysicalMemoryUnsafe"},
    {0x4A, nullptr, "SetUnsafeLimit"},
    {0x4B, nullptr, "CreateCodeMemory"},
    {0x4C, nullptr, "ControlCodeMemory"},
    {0x4D, nullptr, "SleepSystem"},
    {0x4E, nullptr, "ReadWriteRegister"},
    {0x4F, nullptr, "SetProcessActivity"},
    {0x50, nullptr, "CreateSharedMemory"},
    {0x51, nullptr, "MapTransferMemory"},
    {0x52, nullptr, "UnmapTransferMemory"},
    {0x53, nullptr, "CreateInterruptEvent"},
    {0x54, nullptr, "QueryPhysicalAddress"},
    {0x55, nullptr, "QueryIoMapping"},
    {0x56, nullptr, "CreateDeviceAddressSpace"},
    {0x57, nullptr, "AttachDeviceAddressSpace"},
    {0x58, nullptr, "DetachDeviceAddressSpace"},
    {0x59, nullptr, "MapDeviceAddressSpaceByForce"},
    {0x5A, nullptr, "MapDeviceAddressSpaceAligned"},
    {0x5B, nullptr, "MapDeviceAddressSpace"},
    {0x5C, nullptr, "UnmapDeviceAddressSpace"},
    {0x5D, nullptr, "InvalidateProcessDataCache"},
    {0x5E, nullptr, "StoreProcessDataCache"},
    {0x5F, nullptr, "FlushProcessDataCache"},
    {0x60, nullptr, "DebugActiveProcess"},
    {0x61, nullptr, "BreakDebugProcess"},
    {0x62, nullptr, "TerminateDebugProcess"},
    {0x63, nullptr, "GetDebugEvent"},
    {0x64, nullptr, "ContinueDebugEvent"},
    {0x65, SvcWrap64<GetProcessList>, "GetProcessList"},
    {0x66, SvcWrap64<GetThreadList>, "GetThreadList"},
    {0x67, nullptr, "GetDebugThreadContext"},
    {0x68, nullptr, "SetDebugThreadContext"},
    {0x69, nullptr, "QueryDebugProcessMemory"},
    {0x6A, nullptr, "ReadDebugProcessMemory"},
    {0x6B, nullptr, "WriteDebugProcessMemory"},
    {0x6C, nullptr, "SetHardwareBreakPoint"},
    {0x6D, nullptr, "GetDebugThreadParam"},
    {0x6E, nullptr, "Unknown"},
    {0x6F, nullptr, "GetSystemInfo"},
    {0x70, nullptr, "CreatePort"},
    {0x71, nullptr, "ManageNamedPort"},
    {0x72, nullptr, "ConnectToPort"},
    {0x73, nullptr, "SetProcessMemoryPermission"},
    {0x74, nullptr, "MapProcessMemory"},
    {0x75, nullptr, "UnmapProcessMemory"},
    {0x76, SvcWrap64<QueryProcessMemory>, "QueryProcessMemory"},
    {0x77, SvcWrap64<MapProcessCodeMemory>, "MapProcessCodeMemory"},
    {0x78, SvcWrap64<UnmapProcessCodeMemory>, "UnmapProcessCodeMemory"},
    {0x79, nullptr, "CreateProcess"},
    {0x7A, nullptr, "StartProcess"},
    {0x7B, nullptr, "TerminateProcess"},
    {0x7C, SvcWrap64<GetProcessInfo>, "GetProcessInfo"},
    {0x7D, SvcWrap64<CreateResourceLimit>, "CreateResourceLimit"},
    {0x7E, SvcWrap64<SetResourceLimitLimitValue>, "SetResourceLimitLimitValue"},
    {0x7F, nullptr, "CallSecureMonitor"},
};

static const FunctionDef* GetSVCInfo32(u32 func_num) {
    if (func_num >= std::size(SVC_Table_32)) {
        LOG_ERROR(Kernel_SVC, "Unknown svc=0x{:02X}", func_num);
        return nullptr;
    }
    return &SVC_Table_32[func_num];
}

static const FunctionDef* GetSVCInfo64(u32 func_num) {
    if (func_num >= std::size(SVC_Table_64)) {
        LOG_ERROR(Kernel_SVC, "Unknown svc=0x{:02X}", func_num);
        return nullptr;
    }
    return &SVC_Table_64[func_num];
}

void Call(Core::System& system, u32 immediate) {
    system.ExitDynarmicProfile();
    auto& kernel = system.Kernel();
    kernel.EnterSVCProfile();

    const FunctionDef* info = system.CurrentProcess()->Is64BitProcess() ? GetSVCInfo64(immediate)
                                                                        : GetSVCInfo32(immediate);
    if (info) {
        if (info->func) {
            info->func(system);
        } else {
            LOG_CRITICAL(Kernel_SVC, "Unimplemented SVC function {}(..)", info->name);
        }
    } else {
        LOG_CRITICAL(Kernel_SVC, "Unknown SVC function 0x{:X}", immediate);
    }

    kernel.ExitSVCProfile();
    system.EnterDynarmicProfile();
}

} // namespace Kernel::Svc
