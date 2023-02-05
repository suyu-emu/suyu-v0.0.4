// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_client_session.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

/// Makes a blocking IPC call to a service.
Result SendSyncRequest(Core::System& system, Handle handle) {
    auto& kernel = system.Kernel();

    // Get the client session from its handle.
    KScopedAutoObject session =
        kernel.CurrentProcess()->GetHandleTable().GetObject<KClientSession>(handle);
    R_UNLESS(session.IsNotNull(), ResultInvalidHandle);

    LOG_TRACE(Kernel_SVC, "called handle=0x{:08X}({})", handle, session->GetName());

    return session->SendSyncRequest();
}

Result SendSyncRequest32(Core::System& system, Handle handle) {
    return SendSyncRequest(system, handle);
}

Result ReplyAndReceive(Core::System& system, s32* out_index, Handle* handles, s32 num_handles,
                       Handle reply_target, s64 timeout_ns) {
    auto& kernel = system.Kernel();
    auto& handle_table = GetCurrentThread(kernel).GetOwnerProcess()->GetHandleTable();

    // Convert handle list to object table.
    std::vector<KSynchronizationObject*> objs(num_handles);
    R_UNLESS(
        handle_table.GetMultipleObjects<KSynchronizationObject>(objs.data(), handles, num_handles),
        ResultInvalidHandle);

    // Ensure handles are closed when we're done.
    SCOPE_EXIT({
        for (auto i = 0; i < num_handles; ++i) {
            objs[i]->Close();
        }
    });

    // Reply to the target, if one is specified.
    if (reply_target != InvalidHandle) {
        KScopedAutoObject session = handle_table.GetObject<KServerSession>(reply_target);
        R_UNLESS(session.IsNotNull(), ResultInvalidHandle);

        // If we fail to reply, we want to set the output index to -1.
        ON_RESULT_FAILURE {
            *out_index = -1;
        };

        // Send the reply.
        R_TRY(session->SendReply());
    }

    // Wait for a message.
    while (true) {
        // Wait for an object.
        s32 index;
        Result result = KSynchronizationObject::Wait(kernel, &index, objs.data(),
                                                     static_cast<s32>(objs.size()), timeout_ns);
        if (result == ResultTimedOut) {
            return result;
        }

        // Receive the request.
        if (R_SUCCEEDED(result)) {
            KServerSession* session = objs[index]->DynamicCast<KServerSession*>();
            if (session != nullptr) {
                result = session->ReceiveRequest();
                if (result == ResultNotFound) {
                    continue;
                }
            }
        }

        *out_index = index;
        return result;
    }
}

} // namespace Kernel::Svc
