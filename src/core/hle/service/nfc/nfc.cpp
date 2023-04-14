// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "common/logging/log.h"
#include "common/settings.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/nfc/mifare_user.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nfc/nfc_interface.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::NFC {

class IUser final : public Interface {
public:
    explicit IUser(Core::System& system_) : Interface(system_, "IUser") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &Interface::Initialize, "InitializeOld"},
            {1, &Interface::Finalize, "FinalizeOld"},
            {2, &Interface::GetState, "GetStateOld"},
            {3, &Interface::IsNfcEnabled, "IsNfcEnabledOld"},
            {400, &Interface::Initialize, "Initialize"},
            {401, &Interface::Finalize, "Finalize"},
            {402, &Interface::GetState, "GetState"},
            {403, &Interface::IsNfcEnabled, "IsNfcEnabled"},
            {404, &Interface::ListDevices, "ListDevices"},
            {405, &Interface::GetDeviceState, "GetDeviceState"},
            {406, &Interface::GetNpadId, "GetNpadId"},
            {407, &Interface::AttachAvailabilityChangeEvent, "AttachAvailabilityChangeEvent"},
            {408, &Interface::StartDetection, "StartDetection"},
            {409, &Interface::StopDetection, "StopDetection"},
            {410, &Interface::GetTagInfo, "GetTagInfo"},
            {411, &Interface::AttachActivateEvent, "AttachActivateEvent"},
            {412, &Interface::AttachDeactivateEvent, "AttachDeactivateEvent"},
            {1000, nullptr, "ReadMifare"},
            {1001, nullptr, "WriteMifare"},
            {1300, &Interface::SendCommandByPassThrough, "SendCommandByPassThrough"},
            {1301, nullptr, "KeepPassThroughSession"},
            {1302, nullptr, "ReleasePassThroughSession"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class ISystem final : public Interface {
public:
    explicit ISystem(Core::System& system_) : Interface{system_, "ISystem"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &Interface::Initialize, "InitializeOld"},
            {1, &Interface::Finalize, "FinalizeOld"},
            {2, &Interface::GetState, "GetStateOld"},
            {3, &Interface::IsNfcEnabled, "IsNfcEnabledOld"},
            {100, nullptr, "SetNfcEnabledOld"},
            {400, &Interface::Initialize, "Initialize"},
            {401, &Interface::Finalize, "Finalize"},
            {402, &Interface::GetState, "GetState"},
            {403, &Interface::IsNfcEnabled, "IsNfcEnabled"},
            {404, &Interface::ListDevices, "ListDevices"},
            {405, &Interface::GetDeviceState, "GetDeviceState"},
            {406, &Interface::GetNpadId, "GetNpadId"},
            {407, &Interface::AttachAvailabilityChangeEvent, "AttachAvailabilityChangeEvent"},
            {408, &Interface::StartDetection, "StartDetection"},
            {409, &Interface::StopDetection, "StopDetection"},
            {410, &Interface::GetTagInfo, "GetTagInfo"},
            {411, &Interface::AttachActivateEvent, "AttachActivateEvent"},
            {412, &Interface::AttachDeactivateEvent, "AttachDeactivateEvent"},
            {500, nullptr, "SetNfcEnabled"},
            {510, nullptr, "OutputTestWave"},
            {1000, nullptr, "ReadMifare"},
            {1001, nullptr, "WriteMifare"},
            {1300, &Interface::SendCommandByPassThrough, "SendCommandByPassThrough"},
            {1301, nullptr, "KeepPassThroughSession"},
            {1302, nullptr, "ReleasePassThroughSession"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IAm final : public ServiceFramework<IAm> {
public:
    explicit IAm(Core::System& system_) : ServiceFramework{system_, "NFC::IAm"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Initialize"},
            {1, nullptr, "Finalize"},
            {2, nullptr, "NotifyForegroundApplet"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class NFC_AM final : public ServiceFramework<NFC_AM> {
public:
    explicit NFC_AM(Core::System& system_) : ServiceFramework{system_, "nfc:am"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &NFC_AM::CreateAmInterface, "CreateAmInterface"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void CreateAmInterface(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NFC, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IAm>(system);
    }
};

class NFC_MF_U final : public ServiceFramework<NFC_MF_U> {
public:
    explicit NFC_MF_U(Core::System& system_) : ServiceFramework{system_, "nfc:mf:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &NFC_MF_U::CreateUserInterface, "CreateUserInterface"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void CreateUserInterface(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NFC, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<MFIUser>(system);
    }
};

class NFC_U final : public ServiceFramework<NFC_U> {
public:
    explicit NFC_U(Core::System& system_) : ServiceFramework{system_, "nfc:user"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &NFC_U::CreateUserInterface, "CreateUserInterface"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void CreateUserInterface(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NFC, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IUser>(system);
    }
};

class NFC_SYS final : public ServiceFramework<NFC_SYS> {
public:
    explicit NFC_SYS(Core::System& system_) : ServiceFramework{system_, "nfc:sys"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &NFC_SYS::CreateSystemInterface, "CreateSystemInterface"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void CreateSystemInterface(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NFC, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ISystem>(system);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("nfc:am", std::make_shared<NFC_AM>(system));
    server_manager->RegisterNamedService("nfc:mf:u", std::make_shared<NFC_MF_U>(system));
    server_manager->RegisterNamedService("nfc:user", std::make_shared<NFC_U>(system));
    server_manager->RegisterNamedService("nfc:sys", std::make_shared<NFC_SYS>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::NFC
