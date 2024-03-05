// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "core/hle/kernel/k_event.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/npns/npns.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::NPNS {

class INpnsSystem final : public ServiceFramework<INpnsSystem> {
public:
    explicit INpnsSystem(Core::System& system_)
        : ServiceFramework{system_, "npns:s"}, service_context{system, "npns:s"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, nullptr, "ListenAll"},
            {2, C<&INpnsSystem::ListenTo>, "ListenTo"},
            {3, nullptr, "Receive"},
            {4, nullptr, "ReceiveRaw"},
            {5, C<&INpnsSystem::GetReceiveEvent>, "GetReceiveEvent"},
            {6, nullptr, "ListenUndelivered"},
            {7, nullptr, "GetStateChangeEVent"},
            {11, nullptr, "SubscribeTopic"},
            {12, nullptr, "UnsubscribeTopic"},
            {13, nullptr, "QueryIsTopicExist"},
            {21, nullptr, "CreateToken"},
            {22, nullptr, "CreateTokenWithApplicationId"},
            {23, nullptr, "DestroyToken"},
            {24, nullptr, "DestroyTokenWithApplicationId"},
            {25, nullptr, "QueryIsTokenValid"},
            {26, nullptr, "ListenToMyApplicationId"},
            {27, nullptr, "DestroyTokenAll"},
            {31, nullptr, "UploadTokenToBaaS"},
            {32, nullptr, "DestroyTokenForBaaS"},
            {33, nullptr, "CreateTokenForBaaS"},
            {34, nullptr, "SetBaaSDeviceAccountIdList"},
            {101, nullptr, "Suspend"},
            {102, nullptr, "Resume"},
            {103, nullptr, "GetState"},
            {104, nullptr, "GetStatistics"},
            {105, nullptr, "GetPlayReportRequestEvent"},
            {111, nullptr, "GetJid"},
            {112, nullptr, "CreateJid"},
            {113, nullptr, "DestroyJid"},
            {114, nullptr, "AttachJid"},
            {115, nullptr, "DetachJid"},
            {120, nullptr, "CreateNotificationReceiver"},
            {151, nullptr, "GetStateWithHandover"},
            {152, nullptr, "GetStateChangeEventWithHandover"},
            {153, nullptr, "GetDropEventWithHandover"},
            {154, nullptr, "CreateTokenAsync"},
            {155, nullptr, "CreateTokenAsyncWithApplicationId"},
            {161, nullptr, "GetRequestChangeStateCancelEvent"},
            {162, nullptr, "RequestChangeStateForceTimedWithCancelEvent"},
            {201, nullptr, "RequestChangeStateForceTimed"},
            {202, nullptr, "RequestChangeStateForceAsync"},
        };
        // clang-format on

        RegisterHandlers(functions);

        get_receive_event = service_context.CreateEvent("npns:s:GetReceiveEvent");
    }

    ~INpnsSystem() override {
        service_context.CloseEvent(get_receive_event);
    }

private:
    Result ListenTo(u32 program_id) {
        LOG_WARNING(Service_AM, "(STUBBED) called, program_id={}", program_id);
        R_SUCCEED();
    }

    Result GetReceiveEvent(OutCopyHandle<Kernel::KReadableEvent> out_event) {
        LOG_WARNING(Service_AM, "(STUBBED) called");

        *out_event = &get_receive_event->GetReadableEvent();
        R_SUCCEED();
    }

    KernelHelpers::ServiceContext service_context;
    Kernel::KEvent* get_receive_event;
};

class INpnsUser final : public ServiceFramework<INpnsUser> {
public:
    explicit INpnsUser(Core::System& system_) : ServiceFramework{system_, "npns:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, nullptr, "ListenAll"},
            {2, nullptr, "ListenTo"},
            {3, nullptr, "Receive"},
            {4, nullptr, "ReceiveRaw"},
            {5, nullptr, "GetReceiveEvent"},
            {7, nullptr, "GetStateChangeEVent"},
            {21, nullptr, "CreateToken"},
            {23, nullptr, "DestroyToken"},
            {25, nullptr, "QueryIsTokenValid"},
            {26, nullptr, "ListenToMyApplicationId"},
            {101, nullptr, "Suspend"},
            {102, nullptr, "Resume"},
            {103, nullptr, "GetState"},
            {104, nullptr, "GetStatistics"},
            {111, nullptr, "GetJid"},
            {120, nullptr, "CreateNotificationReceiver"},
            {151, nullptr, "GetStateWithHandover"},
            {152, nullptr, "GetStateChangeEventWithHandover"},
            {153, nullptr, "GetDropEventWithHandover"},
            {154, nullptr, "CreateTokenAsync"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("npns:s", std::make_shared<INpnsSystem>(system));
    server_manager->RegisterNamedService("npns:u", std::make_shared<INpnsUser>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::NPNS
