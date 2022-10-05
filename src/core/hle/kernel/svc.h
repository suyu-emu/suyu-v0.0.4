// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/svc_types.h"
#include "core/hle/result.h"

namespace Core {
class System;
}

namespace Kernel::Svc {

void Call(Core::System& system, u32 immediate);

Result SetHeapSize(Core::System& system, VAddr* out_address, u64 size);
Result SetMemoryPermission(Core::System& system, VAddr address, u64 size, MemoryPermission perm);
Result SetMemoryAttribute(Core::System& system, VAddr address, u64 size, u32 mask, u32 attr);
Result MapMemory(Core::System& system, VAddr dst_addr, VAddr src_addr, u64 size);
Result UnmapMemory(Core::System& system, VAddr dst_addr, VAddr src_addr, u64 size);
Result QueryMemory(Core::System& system, VAddr memory_info_address, VAddr page_info_address,
                   VAddr query_address);
void ExitProcess(Core::System& system);
Result CreateThread(Core::System& system, Handle* out_handle, VAddr entry_point, u64 arg,
                    VAddr stack_bottom, u32 priority, s32 core_id);
Result StartThread(Core::System& system, Handle thread_handle);
void ExitThread(Core::System& system);
void SleepThread(Core::System& system, s64 nanoseconds);
Result GetThreadPriority(Core::System& system, u32* out_priority, Handle handle);
Result SetThreadPriority(Core::System& system, Handle thread_handle, u32 priority);
Result GetThreadCoreMask(Core::System& system, Handle thread_handle, s32* out_core_id,
                         u64* out_affinity_mask);
Result SetThreadCoreMask(Core::System& system, Handle thread_handle, s32 core_id,
                         u64 affinity_mask);
u32 GetCurrentProcessorNumber(Core::System& system);
Result SignalEvent(Core::System& system, Handle event_handle);
Result ClearEvent(Core::System& system, Handle event_handle);
Result MapSharedMemory(Core::System& system, Handle shmem_handle, VAddr address, u64 size,
                       MemoryPermission map_perm);
Result UnmapSharedMemory(Core::System& system, Handle shmem_handle, VAddr address, u64 size);
Result CreateTransferMemory(Core::System& system, Handle* out, VAddr address, u64 size,
                            MemoryPermission map_perm);
Result CloseHandle(Core::System& system, Handle handle);
Result ResetSignal(Core::System& system, Handle handle);
Result WaitSynchronization(Core::System& system, s32* index, VAddr handles_address, s32 num_handles,
                           s64 nano_seconds);
Result CancelSynchronization(Core::System& system, Handle handle);
Result ArbitrateLock(Core::System& system, Handle thread_handle, VAddr address, u32 tag);
Result ArbitrateUnlock(Core::System& system, VAddr address);
Result WaitProcessWideKeyAtomic(Core::System& system, VAddr address, VAddr cv_key, u32 tag,
                                s64 timeout_ns);
void SignalProcessWideKey(Core::System& system, VAddr cv_key, s32 count);
u64 GetSystemTick(Core::System& system);
Result ConnectToNamedPort(Core::System& system, Handle* out, VAddr port_name_address);
Result SendSyncRequest(Core::System& system, Handle handle);
Result GetProcessId(Core::System& system, u64* out_process_id, Handle handle);
Result GetThreadId(Core::System& system, u64* out_thread_id, Handle thread_handle);
void Break(Core::System& system, u32 reason, u64 info1, u64 info2);
void OutputDebugString(Core::System& system, VAddr address, u64 len);
Result GetInfo(Core::System& system, u64* result, u64 info_id, Handle handle, u64 info_sub_id);
Result MapPhysicalMemory(Core::System& system, VAddr addr, u64 size);
Result UnmapPhysicalMemory(Core::System& system, VAddr addr, u64 size);
Result GetResourceLimitLimitValue(Core::System& system, u64* out_limit_value,
                                  Handle resource_limit_handle, LimitableResource which);
Result GetResourceLimitCurrentValue(Core::System& system, u64* out_current_value,
                                    Handle resource_limit_handle, LimitableResource which);
Result SetThreadActivity(Core::System& system, Handle thread_handle,
                         ThreadActivity thread_activity);
Result GetThreadContext(Core::System& system, VAddr out_context, Handle thread_handle);
Result WaitForAddress(Core::System& system, VAddr address, ArbitrationType arb_type, s32 value,
                      s64 timeout_ns);
Result SignalToAddress(Core::System& system, VAddr address, SignalType signal_type, s32 value,
                       s32 count);
void SynchronizePreemptionState(Core::System& system);
void KernelDebug(Core::System& system, u32 kernel_debug_type, u64 param1, u64 param2, u64 param3);
void ChangeKernelTraceState(Core::System& system, u32 trace_state);
Result CreateSession(Core::System& system, Handle* out_server, Handle* out_client, u32 is_light,
                     u64 name);
Result ReplyAndReceive(Core::System& system, s32* out_index, Handle* handles, s32 num_handles,
                       Handle reply_target, s64 timeout_ns);
Result CreateEvent(Core::System& system, Handle* out_write, Handle* out_read);
Result CreateCodeMemory(Core::System& system, Handle* out, VAddr address, size_t size);
Result ControlCodeMemory(Core::System& system, Handle code_memory_handle, u32 operation,
                         VAddr address, size_t size, MemoryPermission perm);
Result GetProcessList(Core::System& system, u32* out_num_processes, VAddr out_process_ids,
                      u32 out_process_ids_size);
Result GetThreadList(Core::System& system, u32* out_num_threads, VAddr out_thread_ids,
                     u32 out_thread_ids_size, Handle debug_handle);
Result SetProcessMemoryPermission(Core::System& system, Handle process_handle, VAddr address,
                                  u64 size, MemoryPermission perm);
Result MapProcessMemory(Core::System& system, VAddr dst_address, Handle process_handle,
                        VAddr src_address, u64 size);
Result UnmapProcessMemory(Core::System& system, VAddr dst_address, Handle process_handle,
                          VAddr src_address, u64 size);
Result QueryProcessMemory(Core::System& system, VAddr memory_info_address, VAddr page_info_address,
                          Handle process_handle, VAddr address);
Result MapProcessCodeMemory(Core::System& system, Handle process_handle, u64 dst_address,
                            u64 src_address, u64 size);
Result UnmapProcessCodeMemory(Core::System& system, Handle process_handle, u64 dst_address,
                              u64 src_address, u64 size);
Result GetProcessInfo(Core::System& system, u64* out, Handle process_handle, u32 type);
Result CreateResourceLimit(Core::System& system, Handle* out_handle);
Result SetResourceLimitLimitValue(Core::System& system, Handle resource_limit_handle,
                                  LimitableResource which, u64 limit_value);

//

Result SetHeapSize32(Core::System& system, u32* heap_addr, u32 heap_size);
Result SetMemoryAttribute32(Core::System& system, u32 address, u32 size, u32 mask, u32 attr);
Result MapMemory32(Core::System& system, u32 dst_addr, u32 src_addr, u32 size);
Result UnmapMemory32(Core::System& system, u32 dst_addr, u32 src_addr, u32 size);
Result QueryMemory32(Core::System& system, u32 memory_info_address, u32 page_info_address,
                     u32 query_address);
void ExitProcess32(Core::System& system);
Result CreateThread32(Core::System& system, Handle* out_handle, u32 priority, u32 entry_point,
                      u32 arg, u32 stack_top, s32 processor_id);
Result StartThread32(Core::System& system, Handle thread_handle);
void ExitThread32(Core::System& system);
void SleepThread32(Core::System& system, u32 nanoseconds_low, u32 nanoseconds_high);
Result GetThreadPriority32(Core::System& system, u32* out_priority, Handle handle);
Result SetThreadPriority32(Core::System& system, Handle thread_handle, u32 priority);
Result GetThreadCoreMask32(Core::System& system, Handle thread_handle, s32* out_core_id,
                           u32* out_affinity_mask_low, u32* out_affinity_mask_high);
Result SetThreadCoreMask32(Core::System& system, Handle thread_handle, s32 core_id,
                           u32 affinity_mask_low, u32 affinity_mask_high);
u32 GetCurrentProcessorNumber32(Core::System& system);
Result SignalEvent32(Core::System& system, Handle event_handle);
Result ClearEvent32(Core::System& system, Handle event_handle);
Result MapSharedMemory32(Core::System& system, Handle shmem_handle, u32 address, u32 size,
                         MemoryPermission map_perm);
Result UnmapSharedMemory32(Core::System& system, Handle shmem_handle, u32 address, u32 size);
Result CreateTransferMemory32(Core::System& system, Handle* out, u32 address, u32 size,
                              MemoryPermission map_perm);
Result CloseHandle32(Core::System& system, Handle handle);
Result ResetSignal32(Core::System& system, Handle handle);
Result WaitSynchronization32(Core::System& system, u32 timeout_low, u32 handles_address,
                             s32 num_handles, u32 timeout_high, s32* index);
Result CancelSynchronization32(Core::System& system, Handle handle);
Result ArbitrateLock32(Core::System& system, Handle thread_handle, u32 address, u32 tag);
Result ArbitrateUnlock32(Core::System& system, u32 address);
Result WaitProcessWideKeyAtomic32(Core::System& system, u32 address, u32 cv_key, u32 tag,
                                  u32 timeout_ns_low, u32 timeout_ns_high);
void SignalProcessWideKey32(Core::System& system, u32 cv_key, s32 count);
void GetSystemTick32(Core::System& system, u32* time_low, u32* time_high);
Result ConnectToNamedPort32(Core::System& system, Handle* out_handle, u32 port_name_address);
Result SendSyncRequest32(Core::System& system, Handle handle);
Result GetProcessId32(Core::System& system, u32* out_process_id_low, u32* out_process_id_high,
                      Handle handle);
Result GetThreadId32(Core::System& system, u32* out_thread_id_low, u32* out_thread_id_high,
                     Handle thread_handle);
void Break32(Core::System& system, u32 reason, u32 info1, u32 info2);
void OutputDebugString32(Core::System& system, u32 address, u32 len);
Result GetInfo32(Core::System& system, u32* result_low, u32* result_high, u32 sub_id_low,
                 u32 info_id, u32 handle, u32 sub_id_high);
Result MapPhysicalMemory32(Core::System& system, u32 addr, u32 size);
Result UnmapPhysicalMemory32(Core::System& system, u32 addr, u32 size);
Result SetThreadActivity32(Core::System& system, Handle thread_handle,
                           ThreadActivity thread_activity);
Result GetThreadContext32(Core::System& system, u32 out_context, Handle thread_handle);
Result WaitForAddress32(Core::System& system, u32 address, ArbitrationType arb_type, s32 value,
                        u32 timeout_ns_low, u32 timeout_ns_high);
Result SignalToAddress32(Core::System& system, u32 address, SignalType signal_type, s32 value,
                         s32 count);
Result CreateEvent32(Core::System& system, Handle* out_write, Handle* out_read);
Result CreateCodeMemory32(Core::System& system, Handle* out, u32 address, u32 size);
Result ControlCodeMemory32(Core::System& system, Handle code_memory_handle, u32 operation,
                           u64 address, u64 size, MemoryPermission perm);
Result FlushProcessDataCache32(Core::System& system, Handle process_handle, u64 address, u64 size);

} // namespace Kernel::Svc
