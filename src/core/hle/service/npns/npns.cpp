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
            {7, nullptr, "GetStateChangeEvent"},
            {8, C<&INpnsSystem::ListenToByName>, "ListenToByName"},
            {11, nullptr, "SubscribeTopic"},
            {12, nullptr, "UnsubscribeTopic"},
            {13, nullptr, "QueryIsTopicExist"},
            {14, nullptr, "SubscribeTopicByAccount"}, // 18.0.0+
            {15, nullptr, "UnsubscribeTopicByAccount"}, // 18.0.0+
            {16, nullptr, "DownloadSubscriptionList"}, // 18.0.0+
            {21, nullptr, "CreateToken"},
            {22, nullptr, "CreateTokenWithApplicationId"},
            {23, nullptr, "DestroyToken"},
            {24, nullptr, "DestroyTokenWithApplicationId"},
            {25, nullptr, "QueryIsTokenValid"},
            {26, nullptr, "ListenToMyApplicationId"},
            {27, nullptr, "DestroyTokenAll"},
            {28, nullptr, "CreateTokenWithName"}, // 18.0.0+
            {29, nullptr, "DestroyTokenWithName"}, // 18.0.0+
            {31, nullptr, "UploadTokenToBaaS"},
            {32, nullptr, "DestroyTokenForBaaS"},
            {33, nullptr, "CreateTokenForBaaS"},
            {34, nullptr, "SetBaaSDeviceAccountIdList"},
            {35, nullptr, "LinkNsaId"}, // 17.0.0+
            {36, nullptr, "UnlinkNsaId"}, // 17.0.0+
            {37, nullptr, "RelinkNsaId"}, // 18.0.0+
            {40, nullptr, "GetNetworkServiceAccountIdTokenRequestEvent"}, // 17.0.0+
            {41, nullptr, "TryPopNetworkServiceAccountIdTokenRequestUid"}, // 17.0.0+
            {42, nullptr, "SetNetworkServiceAccountIdTokenSuccess"}, // 17.0.0+
            {43, nullptr, "SetNetworkServiceAccountIdTokenFailure"}, // 17.0.0+
            {44, nullptr, "SetUidList"}, // 17.0.0+
            {45, nullptr, "PutDigitalTwinKeyValue"}, // 17.0.0+
            {51, nullptr, "DeleteDigitalTwinKeyValue"}, // 18.0.0+
            {101, nullptr, "Suspend"},
            {102, nullptr, "Resume"},
            {103, C<&INpnsSystem::GetState>, "GetState"},
            {104, nullptr, "GetStatistics"},
            {105, nullptr, "GetPlayReportRequestEvent"},
            {106, C<&INpnsSystem::GetLastNotifiedTime>, "GetLastNotifiedTime"}, // 18.0.0+
            {107, nullptr, "SetLastNotifiedTime"}, // 18.0.0+
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
            {156, nullptr, "CreateTokenWithNameAsync"}, // 18.0.0+
            {161, C<&INpnsSystem::GetRequestChangeStateCancelEvent>, "GetRequestChangeStateCancelEvent"}, // 10.0.0+
            {162, nullptr, "RequestChangeStateForceTimedWithCancelEvent"},
            {201, nullptr, "RequestChangeStateForceTimed"},
            {202, nullptr, "RequestChangeStateForceAsync"},
            {301, nullptr, "GetPassword"}, // 18.0.0+
            {302, nullptr, "GetAllImmigration"}, // 18.0.0+
            {303, nullptr, "GetNotificationHistories"}, // 18.0.0+
            {304, nullptr, "GetPersistentConnectionSummary"}, // 18.0.0+
            {305, nullptr, "GetDigitalTwinSummary"}, // 18.0.0+
            {306, nullptr, "GetDigitalTwinValue"}, // 18.0.0+
        };
        // clang-format on

        RegisterHandlers(functions);

        get_receive_event = service_context.CreateEvent("npns:s:GetReceiveEvent");
        get_request_change_state_cancel_event =
            service_context.CreateEvent("npns:s:GetRequestChangeStateCancelEvent");
    }

    ~INpnsSystem() override {
        service_context.CloseEvent(get_receive_event);
        service_context.CloseEvent(get_request_change_state_cancel_event);
    }

private:
    Result ListenTo(u32 program_id) {
        LOG_WARNING(Service_NPNS, "(STUBBED) called, program_id={}", program_id);
        R_SUCCEED();
    }

    Result GetReceiveEvent(OutCopyHandle<Kernel::KReadableEvent> out_event) {
        LOG_WARNING(Service_NPNS, "(STUBBED) called");

        *out_event = &get_receive_event->GetReadableEvent();
        R_SUCCEED();
    }

    Result ListenToByName() {
        LOG_DEBUG(Service_NPNS, "(STUBBED) called.");

        // TODO (jarrodnorwell)

        R_SUCCEED();
    }

    Result GetState(Out<u32> out_state) {
        LOG_WARNING(Service_NPNS, "(STUBBED) called");
        *out_state = 0;
        R_SUCCEED();
    }

    Result GetLastNotifiedTime(Out<s64> out_last_notified_time) {
        LOG_WARNING(Service_NPNS, "(STUBBED) called");

        *out_last_notified_time = 0;
        R_SUCCEED();
    }

    Result GetRequestChangeStateCancelEvent(OutCopyHandle<Kernel::KReadableEvent> out_event) {
        LOG_DEBUG(Service_NPNS, "(STUBBED) called.");

        // TODO (jarrodnorwell)

        *out_event = &get_request_change_state_cancel_event->GetReadableEvent();

        R_SUCCEED();
    }

    KernelHelpers::ServiceContext service_context;
    Kernel::KEvent* get_receive_event;
    Kernel::KEvent* get_request_change_state_cancel_event;
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
            {7, nullptr, "GetStateChangeEvent"},
            {8, nullptr, "ListenToByName"}, // 18.0.0+
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
