// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_client_session.h"
#include "core/hle/kernel/k_object_name.h"
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
    auto& handle_table = GetCurrentProcess(kernel).GetHandleTable();

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

Result CreatePort(Core::System& system, Handle* out_server, Handle* out_client,
                  int32_t max_sessions, bool is_light, uint64_t name) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result ConnectToPort(Core::System& system, Handle* out_handle, Handle port) {
    UNIMPLEMENTED();
    R_THROW(ResultNotImplemented);
}

Result ManageNamedPort(Core::System& system, Handle* out_server_handle, uint64_t user_name,
                       int32_t max_sessions) {
    // Copy the provided name from user memory to kernel memory.
    std::array<char, KObjectName::NameLengthMax> name{};
    system.Memory().ReadBlock(user_name, name.data(), sizeof(name));

    // Validate that sessions and name are valid.
    R_UNLESS(max_sessions >= 0, ResultOutOfRange);
    R_UNLESS(name[sizeof(name) - 1] == '\x00', ResultOutOfRange);

    if (max_sessions > 0) {
        // Get the current handle table.
        auto& handle_table = GetCurrentProcess(system.Kernel()).GetHandleTable();

        // Create a new port.
        KPort* port = KPort::Create(system.Kernel());
        R_UNLESS(port != nullptr, ResultOutOfResource);

        // Initialize the new port.
        port->Initialize(max_sessions, false, "");

        // Register the port.
        KPort::Register(system.Kernel(), port);

        // Ensure that our only reference to the port is in the handle table when we're done.
        SCOPE_EXIT({
            port->GetClientPort().Close();
            port->GetServerPort().Close();
        });

        // Register the handle in the table.
        R_TRY(handle_table.Add(out_server_handle, std::addressof(port->GetServerPort())));
        ON_RESULT_FAILURE {
            handle_table.Remove(*out_server_handle);
        };

        // Create a new object name.
        R_TRY(KObjectName::NewFromName(system.Kernel(), std::addressof(port->GetClientPort()),
                                       name.data()));
    } else /* if (max_sessions == 0) */ {
        // Ensure that this else case is correct.
        ASSERT(max_sessions == 0);

        // If we're closing, there's no server handle.
        *out_server_handle = InvalidHandle;

        // Delete the object.
        R_TRY(KObjectName::Delete<KClientPort>(system.Kernel(), name.data()));
    }

    R_SUCCEED();
}

Result ConnectToNamedPort64(Core::System& system, Handle* out_handle, uint64_t name) {
    R_RETURN(ConnectToNamedPort(system, out_handle, name));
}

Result CreatePort64(Core::System& system, Handle* out_server_handle, Handle* out_client_handle,
                    int32_t max_sessions, bool is_light, uint64_t name) {
    R_RETURN(
        CreatePort(system, out_server_handle, out_client_handle, max_sessions, is_light, name));
}

Result ManageNamedPort64(Core::System& system, Handle* out_server_handle, uint64_t name,
                         int32_t max_sessions) {
    R_RETURN(ManageNamedPort(system, out_server_handle, name, max_sessions));
}

Result ConnectToPort64(Core::System& system, Handle* out_handle, Handle port) {
    R_RETURN(ConnectToPort(system, out_handle, port));
}

Result ConnectToNamedPort64From32(Core::System& system, Handle* out_handle, uint32_t name) {
    R_RETURN(ConnectToNamedPort(system, out_handle, name));
}

Result CreatePort64From32(Core::System& system, Handle* out_server_handle,
                          Handle* out_client_handle, int32_t max_sessions, bool is_light,
                          uint32_t name) {
    R_RETURN(
        CreatePort(system, out_server_handle, out_client_handle, max_sessions, is_light, name));
}

Result ManageNamedPort64From32(Core::System& system, Handle* out_server_handle, uint32_t name,
                               int32_t max_sessions) {
    R_RETURN(ManageNamedPort(system, out_server_handle, name, max_sessions));
}

Result ConnectToPort64From32(Core::System& system, Handle* out_handle, Handle port) {
    R_RETURN(ConnectToPort(system, out_handle, port));
}

} // namespace Kernel::Svc
