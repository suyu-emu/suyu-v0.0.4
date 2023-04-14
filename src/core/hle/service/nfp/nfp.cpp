// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/nfp/nfp.h"
#include "core/hle/service/nfp/nfp_interface.h"
#include "core/hle/service/server_manager.h"

namespace Service::NFP {

class IUser final : public Interface {
public:
    explicit IUser(Core::System& system_) : Interface(system_, "NFP:IUser") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IUser::Initialize, "Initialize"},
            {1, &IUser::Finalize, "Finalize"},
            {2, &IUser::ListDevices, "ListDevices"},
            {3, &IUser::StartDetection, "StartDetection"},
            {4, &IUser::StopDetection, "StopDetection"},
            {5, &IUser::Mount, "Mount"},
            {6, &IUser::Unmount, "Unmount"},
            {7, &IUser::OpenApplicationArea, "OpenApplicationArea"},
            {8, &IUser::GetApplicationArea, "GetApplicationArea"},
            {9, &IUser::SetApplicationArea, "SetApplicationArea"},
            {10, &IUser::Flush, "Flush"},
            {11, &IUser::Restore, "Restore"},
            {12, &IUser::CreateApplicationArea, "CreateApplicationArea"},
            {13, &IUser::GetTagInfo, "GetTagInfo"},
            {14, &IUser::GetRegisterInfo, "GetRegisterInfo"},
            {15, &IUser::GetCommonInfo, "GetCommonInfo"},
            {16, &IUser::GetModelInfo, "GetModelInfo"},
            {17, &IUser::AttachActivateEvent, "AttachActivateEvent"},
            {18, &IUser::AttachDeactivateEvent, "AttachDeactivateEvent"},
            {19, &IUser::GetState, "GetState"},
            {20, &IUser::GetDeviceState, "GetDeviceState"},
            {21, &IUser::GetNpadId, "GetNpadId"},
            {22, &IUser::GetApplicationAreaSize, "GetApplicationAreaSize"},
            {23, &IUser::AttachAvailabilityChangeEvent, "AttachAvailabilityChangeEvent"},
            {24, &IUser::RecreateApplicationArea, "RecreateApplicationArea"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class ISystem final : public Interface {
public:
    explicit ISystem(Core::System& system_) : Interface(system_, "NFP:ISystem") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "InitializeSystem"},
            {1, nullptr, "FinalizeSystem"},
            {2, &ISystem::ListDevices, "ListDevices"},
            {3, &ISystem::StartDetection, "StartDetection"},
            {4, &ISystem::StopDetection, "StopDetection"},
            {5, &ISystem::Mount, "Mount"},
            {6, &ISystem::Unmount, "Unmount"},
            {10, &ISystem::Flush, "Flush"},
            {11, &ISystem::Restore, "Restore"},
            {12, &ISystem::CreateApplicationArea, "CreateApplicationArea"},
            {13, &ISystem::GetTagInfo, "GetTagInfo"},
            {14, &ISystem::GetRegisterInfo, "GetRegisterInfo"},
            {15, &ISystem::GetCommonInfo, "GetCommonInfo"},
            {16, &ISystem::GetModelInfo, "GetModelInfo"},
            {17, &ISystem::AttachActivateEvent, "AttachActivateEvent"},
            {18, &ISystem::AttachDeactivateEvent, "AttachDeactivateEvent"},
            {19, &ISystem::GetState, "GetState"},
            {20, &ISystem::GetDeviceState, "GetDeviceState"},
            {21, &ISystem::GetNpadId, "GetNpadId"},
            {23, &ISystem::AttachAvailabilityChangeEvent, "AttachAvailabilityChangeEvent"},
            {100, nullptr, "Format"},
            {101, nullptr, "GetAdminInfo"},
            {102, nullptr, "GetRegisterInfoPrivate"},
            {103, nullptr, "SetRegisterInfoPrivate"},
            {104, nullptr, "DeleteRegisterInfo"},
            {105, nullptr, "DeleteApplicationArea"},
            {106, nullptr, "ExistsApplicationArea"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IDebug final : public Interface {
public:
    explicit IDebug(Core::System& system_) : Interface(system_, "NFP:IDebug") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "InitializeDebug"},
            {1, nullptr, "FinalizeDebug"},
            {2, &IDebug::ListDevices, "ListDevices"},
            {3, &IDebug::StartDetection, "StartDetection"},
            {4, &IDebug::StopDetection, "StopDetection"},
            {5, &IDebug::Mount, "Mount"},
            {6, &IDebug::Unmount, "Unmount"},
            {7, &IDebug::OpenApplicationArea, "OpenApplicationArea"},
            {8, &IDebug::GetApplicationArea, "GetApplicationArea"},
            {9, &IDebug::SetApplicationArea, "SetApplicationArea"},
            {10, &IDebug::Flush, "Flush"},
            {11, &IDebug::Restore, "Restore"},
            {12, &IDebug::CreateApplicationArea, "CreateApplicationArea"},
            {13, &IDebug::GetTagInfo, "GetTagInfo"},
            {14, &IDebug::GetRegisterInfo, "GetRegisterInfo"},
            {15, &IDebug::GetCommonInfo, "GetCommonInfo"},
            {16, &IDebug::GetModelInfo, "GetModelInfo"},
            {17, &IDebug::AttachActivateEvent, "AttachActivateEvent"},
            {18, &IDebug::AttachDeactivateEvent, "AttachDeactivateEvent"},
            {19, &IDebug::GetState, "GetState"},
            {20, &IDebug::GetDeviceState, "GetDeviceState"},
            {21, &IDebug::GetNpadId, "GetNpadId"},
            {22, &IDebug::GetApplicationAreaSize, "GetApplicationAreaSize"},
            {23, &IDebug::AttachAvailabilityChangeEvent, "AttachAvailabilityChangeEvent"},
            {24, &IDebug::RecreateApplicationArea, "RecreateApplicationArea"},
            {100, nullptr, "Format"},
            {101, nullptr, "GetAdminInfo"},
            {102, nullptr, "GetRegisterInfoPrivate"},
            {103, nullptr, "SetRegisterInfoPrivate"},
            {104, nullptr, "DeleteRegisterInfo"},
            {105, nullptr, "DeleteApplicationArea"},
            {106, nullptr, "ExistsApplicationArea"},
            {200, nullptr, "GetAll"},
            {201, nullptr, "SetAll"},
            {202, nullptr, "FlushDebug"},
            {203, nullptr, "BreakTag"},
            {204, nullptr, "ReadBackupData"},
            {205, nullptr, "WriteBackupData"},
            {206, nullptr, "WriteNtf"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IUserManager final : public ServiceFramework<IUserManager> {
public:
    explicit IUserManager(Core::System& system_) : ServiceFramework{system_, "nfp:user"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IUserManager::CreateUserInterface, "CreateUserInterface"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void CreateUserInterface(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NFP, "called");

        if (user_interface == nullptr) {
            user_interface = std::make_shared<IUser>(system);
        }

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IUser>(user_interface);
    }

    std::shared_ptr<IUser> user_interface;
};

class ISystemManager final : public ServiceFramework<ISystemManager> {
public:
    explicit ISystemManager(Core::System& system_) : ServiceFramework{system_, "nfp:sys"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &ISystemManager::CreateSystemInterface, "CreateSystemInterface"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void CreateSystemInterface(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NFP, "called");

        if (system_interface == nullptr) {
            system_interface = std::make_shared<ISystem>(system);
        }

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ISystem>(system_interface);
    }

    std::shared_ptr<ISystem> system_interface;
};

class IDebugManager final : public ServiceFramework<IDebugManager> {
public:
    explicit IDebugManager(Core::System& system_) : ServiceFramework{system_, "nfp:dbg"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IDebugManager::CreateDebugInterface, "CreateDebugInterface"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void CreateDebugInterface(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NFP, "called");

        if (system_interface == nullptr) {
            system_interface = std::make_shared<IDebug>(system);
        }

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IDebug>(system_interface);
    }

    std::shared_ptr<IDebug> system_interface;
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("nfp:user", std::make_shared<IUserManager>(system));
    server_manager->RegisterNamedService("nfp:sys", std::make_shared<ISystemManager>(system));
    server_manager->RegisterNamedService("nfp:dbg", std::make_shared<IDebugManager>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::NFP
