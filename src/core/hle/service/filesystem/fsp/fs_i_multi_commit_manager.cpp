// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/filesystem/fsp/fs_i_multi_commit_manager.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::FileSystem {

IMultiCommitManager::IMultiCommitManager(Core::System& system_)
    : ServiceFramework{system_, "IMultiCommitManager"} {
    static const FunctionInfo functions[] = {
        {1, &IMultiCommitManager::Add, "Add"},
        {2, &IMultiCommitManager::Commit, "Commit"},
    };
    RegisterHandlers(functions);
}

IMultiCommitManager::~IMultiCommitManager() = default;

void IMultiCommitManager::Add(HLERequestContext& ctx) {
    LOG_WARNING(Service_FS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IMultiCommitManager::Commit(HLERequestContext& ctx) {
    LOG_WARNING(Service_FS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

} // namespace Service::FileSystem
