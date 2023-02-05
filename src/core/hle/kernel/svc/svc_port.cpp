// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_client_session.h"
#include "core/hle/kernel/k_port.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

/// Connect to an OS service given the port name, returns the handle to the port to out
Result ConnectToNamedPort(Core::System& system, Handle* out, VAddr port_name_address) {
    auto& memory = system.Memory();
    if (!memory.IsValidVirtualAddress(port_name_address)) {
        LOG_ERROR(Kernel_SVC,
                  "Port Name Address is not a valid virtual address, port_name_address=0x{:016X}",
                  port_name_address);
        return ResultNotFound;
    }

    static constexpr std::size_t PortNameMaxLength = 11;
    // Read 1 char beyond the max allowed port name to detect names that are too long.
    const std::string port_name = memory.ReadCString(port_name_address, PortNameMaxLength + 1);
    if (port_name.size() > PortNameMaxLength) {
        LOG_ERROR(Kernel_SVC, "Port name is too long, expected {} but got {}", PortNameMaxLength,
                  port_name.size());
        return ResultOutOfRange;
    }

    LOG_TRACE(Kernel_SVC, "called port_name={}", port_name);

    // Get the current handle table.
    auto& kernel = system.Kernel();
    auto& handle_table = kernel.CurrentProcess()->GetHandleTable();

    // Find the client port.
    auto port = kernel.CreateNamedServicePort(port_name);
    if (!port) {
        LOG_ERROR(Kernel_SVC, "tried to connect to unknown port: {}", port_name);
        return ResultNotFound;
    }

    // Reserve a handle for the port.
    // NOTE: Nintendo really does write directly to the output handle here.
    R_TRY(handle_table.Reserve(out));
    auto handle_guard = SCOPE_GUARD({ handle_table.Unreserve(*out); });

    // Create a session.
    KClientSession* session{};
    R_TRY(port->CreateSession(std::addressof(session)));

    kernel.RegisterNamedServiceHandler(port_name, &port->GetParent()->GetServerPort());

    // Register the session in the table, close the extra reference.
    handle_table.Register(*out, session);
    session->Close();

    // We succeeded.
    handle_guard.Cancel();
    return ResultSuccess;
}

Result ConnectToNamedPort32(Core::System& system, Handle* out_handle, u32 port_name_address) {

    return ConnectToNamedPort(system, out_handle, port_name_address);
}

} // namespace Kernel::Svc
