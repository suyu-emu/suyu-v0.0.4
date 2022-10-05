// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

Result QueryMemory(Core::System& system, VAddr memory_info_address, VAddr page_info_address,
                   VAddr query_address) {
    LOG_TRACE(Kernel_SVC,
              "called, memory_info_address=0x{:016X}, page_info_address=0x{:016X}, "
              "query_address=0x{:016X}",
              memory_info_address, page_info_address, query_address);

    return QueryProcessMemory(system, memory_info_address, page_info_address, CurrentProcess,
                              query_address);
}

Result QueryMemory32(Core::System& system, u32 memory_info_address, u32 page_info_address,
                     u32 query_address) {
    return QueryMemory(system, memory_info_address, page_info_address, query_address);
}

Result QueryProcessMemory(Core::System& system, VAddr memory_info_address, VAddr page_info_address,
                          Handle process_handle, VAddr address) {
    LOG_TRACE(Kernel_SVC, "called process=0x{:08X} address={:X}", process_handle, address);
    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();
    KScopedAutoObject process = handle_table.GetObject<KProcess>(process_handle);
    if (process.IsNull()) {
        LOG_ERROR(Kernel_SVC, "Process handle does not exist, process_handle=0x{:08X}",
                  process_handle);
        return ResultInvalidHandle;
    }

    auto& memory{system.Memory()};
    const auto memory_info{process->PageTable().QueryInfo(address).GetSvcMemoryInfo()};

    memory.Write64(memory_info_address + 0x00, memory_info.base_address);
    memory.Write64(memory_info_address + 0x08, memory_info.size);
    memory.Write32(memory_info_address + 0x10, static_cast<u32>(memory_info.state) & 0xff);
    memory.Write32(memory_info_address + 0x14, static_cast<u32>(memory_info.attribute));
    memory.Write32(memory_info_address + 0x18, static_cast<u32>(memory_info.permission));
    memory.Write32(memory_info_address + 0x1c, memory_info.ipc_count);
    memory.Write32(memory_info_address + 0x20, memory_info.device_count);
    memory.Write32(memory_info_address + 0x24, 0);

    // Page info appears to be currently unused by the kernel and is always set to zero.
    memory.Write32(page_info_address, 0);

    return ResultSuccess;
}

} // namespace Kernel::Svc
