// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

/// Exits the current process
void ExitProcess(Core::System& system) {
    auto* current_process = system.Kernel().CurrentProcess();

    LOG_INFO(Kernel_SVC, "Process {} exiting", current_process->GetProcessID());
    ASSERT_MSG(current_process->GetState() == KProcess::State::Running,
               "Process has already exited");

    system.Exit();
}

void ExitProcess32(Core::System& system) {
    ExitProcess(system);
}

/// Gets the ID of the specified process or a specified thread's owning process.
Result GetProcessId(Core::System& system, u64* out_process_id, Handle handle) {
    LOG_DEBUG(Kernel_SVC, "called handle=0x{:08X}", handle);

    // Get the object from the handle table.
    KScopedAutoObject obj =
        system.Kernel().CurrentProcess()->GetHandleTable().GetObject<KAutoObject>(
            static_cast<Handle>(handle));
    R_UNLESS(obj.IsNotNull(), ResultInvalidHandle);

    // Get the process from the object.
    KProcess* process = nullptr;
    if (KProcess* p = obj->DynamicCast<KProcess*>(); p != nullptr) {
        // The object is a process, so we can use it directly.
        process = p;
    } else if (KThread* t = obj->DynamicCast<KThread*>(); t != nullptr) {
        // The object is a thread, so we want to use its parent.
        process = reinterpret_cast<KThread*>(obj.GetPointerUnsafe())->GetOwnerProcess();
    } else {
        // TODO(bunnei): This should also handle debug objects before returning.
        UNIMPLEMENTED_MSG("Debug objects not implemented");
    }

    // Make sure the target process exists.
    R_UNLESS(process != nullptr, ResultInvalidHandle);

    // Get the process id.
    *out_process_id = process->GetId();

    return ResultSuccess;
}

Result GetProcessId32(Core::System& system, u32* out_process_id_low, u32* out_process_id_high,
                      Handle handle) {
    u64 out_process_id{};
    const auto result = GetProcessId(system, &out_process_id, handle);
    *out_process_id_low = static_cast<u32>(out_process_id);
    *out_process_id_high = static_cast<u32>(out_process_id >> 32);
    return result;
}

Result GetProcessList(Core::System& system, u32* out_num_processes, VAddr out_process_ids,
                      u32 out_process_ids_size) {
    LOG_DEBUG(Kernel_SVC, "called. out_process_ids=0x{:016X}, out_process_ids_size={}",
              out_process_ids, out_process_ids_size);

    // If the supplied size is negative or greater than INT32_MAX / sizeof(u64), bail.
    if ((out_process_ids_size & 0xF0000000) != 0) {
        LOG_ERROR(Kernel_SVC,
                  "Supplied size outside [0, 0x0FFFFFFF] range. out_process_ids_size={}",
                  out_process_ids_size);
        return ResultOutOfRange;
    }

    const auto& kernel = system.Kernel();
    const auto total_copy_size = out_process_ids_size * sizeof(u64);

    if (out_process_ids_size > 0 && !kernel.CurrentProcess()->PageTable().IsInsideAddressSpace(
                                        out_process_ids, total_copy_size)) {
        LOG_ERROR(Kernel_SVC, "Address range outside address space. begin=0x{:016X}, end=0x{:016X}",
                  out_process_ids, out_process_ids + total_copy_size);
        return ResultInvalidCurrentMemory;
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
    return ResultSuccess;
}

Result GetProcessInfo(Core::System& system, u64* out, Handle process_handle, u32 type) {
    LOG_DEBUG(Kernel_SVC, "called, handle=0x{:08X}, type=0x{:X}", process_handle, type);

    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    KScopedAutoObject process = handle_table.GetObject<KProcess>(process_handle);
    if (process.IsNull()) {
        LOG_ERROR(Kernel_SVC, "Process handle does not exist, process_handle=0x{:08X}",
                  process_handle);
        return ResultInvalidHandle;
    }

    const auto info_type = static_cast<ProcessInfoType>(type);
    if (info_type != ProcessInfoType::ProcessState) {
        LOG_ERROR(Kernel_SVC, "Expected info_type to be ProcessState but got {} instead", type);
        return ResultInvalidEnumValue;
    }

    *out = static_cast<u64>(process->GetState());
    return ResultSuccess;
}

} // namespace Kernel::Svc
