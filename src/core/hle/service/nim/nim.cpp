// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <ctime>
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/nim/nim.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::NIM {

class NIM final : public ServiceFramework<NIM> {
public:
    explicit NIM() : ServiceFramework{"nim"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "CreateSystemUpdateTask"},
            {1, nullptr, "DestroySystemUpdateTask"},
            {2, nullptr, "ListSystemUpdateTask"},
            {3, nullptr, "RequestSystemUpdateTaskRun"},
            {4, nullptr, "GetSystemUpdateTaskInfo"},
            {5, nullptr, "CommitSystemUpdateTask"},
            {6, nullptr, "CreateNetworkInstallTask"},
            {7, nullptr, "DestroyNetworkInstallTask"},
            {8, nullptr, "ListNetworkInstallTask"},
            {9, nullptr, "RequestNetworkInstallTaskRun"},
            {10, nullptr, "GetNetworkInstallTaskInfo"},
            {11, nullptr, "CommitNetworkInstallTask"},
            {12, nullptr, "RequestLatestSystemUpdateMeta"},
            {14, nullptr, "ListApplicationNetworkInstallTask"},
            {15, nullptr, "ListNetworkInstallTaskContentMeta"},
            {16, nullptr, "RequestLatestVersion"},
            {17, nullptr, "SetNetworkInstallTaskAttribute"},
            {18, nullptr, "AddNetworkInstallTaskContentMeta"},
            {19, nullptr, "GetDownloadedSystemDataPath"},
            {20, nullptr, "CalculateNetworkInstallTaskRequiredSize"},
            {21, nullptr, "IsExFatDriverIncluded"},
            {22, nullptr, "GetBackgroundDownloadStressTaskInfo"},
            {23, nullptr, "RequestDeviceAuthenticationToken"},
            {24, nullptr, "RequestGameCardRegistrationStatus"},
            {25, nullptr, "RequestRegisterGameCard"},
            {26, nullptr, "RequestRegisterNotificationToken"},
            {27, nullptr, "RequestDownloadTaskList"},
            {28, nullptr, "RequestApplicationControl"},
            {29, nullptr, "RequestLatestApplicationControl"},
            {30, nullptr, "RequestVersionList"},
            {31, nullptr, "CreateApplyDeltaTask"},
            {32, nullptr, "DestroyApplyDeltaTask"},
            {33, nullptr, "ListApplicationApplyDeltaTask"},
            {34, nullptr, "RequestApplyDeltaTaskRun"},
            {35, nullptr, "GetApplyDeltaTaskInfo"},
            {36, nullptr, "ListApplyDeltaTask"},
            {37, nullptr, "CommitApplyDeltaTask"},
            {38, nullptr, "CalculateApplyDeltaTaskRequiredSize"},
            {39, nullptr, "PrepareShutdown"},
            {40, nullptr, "ListApplyDeltaTask"},
            {41, nullptr, "ClearNotEnoughSpaceStateOfApplyDeltaTask"},
            {42, nullptr, "Unknown1"},
            {43, nullptr, "Unknown2"},
            {44, nullptr, "Unknown3"},
            {45, nullptr, "Unknown4"},
            {46, nullptr, "Unknown5"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class NIM_SHP final : public ServiceFramework<NIM_SHP> {
public:
    explicit NIM_SHP() : ServiceFramework{"nim:shp"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "RequestDeviceAuthenticationToken"},
            {1, nullptr, "RequestCachedDeviceAuthenticationToken"},
            {100, nullptr, "RequestRegisterDeviceAccount"},
            {101, nullptr, "RequestUnregisterDeviceAccount"},
            {102, nullptr, "RequestDeviceAccountStatus"},
            {103, nullptr, "GetDeviceAccountInfo"},
            {104, nullptr, "RequestDeviceRegistrationInfo"},
            {105, nullptr, "RequestTransferDeviceAccount"},
            {106, nullptr, "RequestSyncRegistration"},
            {107, nullptr, "IsOwnDeviceId"},
            {200, nullptr, "RequestRegisterNotificationToken"},
            {300, nullptr, "RequestUnlinkDevice"},
            {301, nullptr, "RequestUnlinkDeviceIntegrated"},
            {302, nullptr, "RequestLinkDevice"},
            {303, nullptr, "HasDeviceLink"},
            {304, nullptr, "RequestUnlinkDeviceAll"},
            {305, nullptr, "RequestCreateVirtualAccount"},
            {306, nullptr, "RequestDeviceLinkStatus"},
            {400, nullptr, "GetAccountByVirtualAccount"},
            {500, nullptr, "RequestSyncTicket"},
            {501, nullptr, "RequestDownloadTicket"},
            {502, nullptr, "RequestDownloadTicketForPrepurchasedContents"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IEnsureNetworkClockAvailabilityService final
    : public ServiceFramework<IEnsureNetworkClockAvailabilityService> {
public:
    IEnsureNetworkClockAvailabilityService()
        : ServiceFramework("IEnsureNetworkClockAvailabilityService") {
        static const FunctionInfo functions[] = {
            {0, &IEnsureNetworkClockAvailabilityService::StartTask, "StartTask"},
            {1, &IEnsureNetworkClockAvailabilityService::GetFinishNotificationEvent,
             "GetFinishNotificationEvent"},
            {2, &IEnsureNetworkClockAvailabilityService::GetResult, "GetResult"},
            {3, &IEnsureNetworkClockAvailabilityService::Cancel, "Cancel"},
            {4, &IEnsureNetworkClockAvailabilityService::IsProcessing, "IsProcessing"},
            {5, &IEnsureNetworkClockAvailabilityService::GetServerTime, "GetServerTime"},
        };
        RegisterHandlers(functions);

        auto& kernel = Core::System::GetInstance().Kernel();
        finished_event =
            Kernel::Event::Create(kernel, Kernel::ResetType::OneShot,
                                  "IEnsureNetworkClockAvailabilityService:FinishEvent");
    }

private:
    Kernel::SharedPtr<Kernel::Event> finished_event;

    void StartTask(Kernel::HLERequestContext& ctx) {
        // No need to connect to the internet, just finish the task straight away.
        finished_event->Signal();
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_DEBUG(Service_NIM, "called");
    }

    void GetFinishNotificationEvent(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(finished_event);
        LOG_DEBUG(Service_NIM, "called");
    }

    void GetResult(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_DEBUG(Service_NIM, "called");
    }

    void Cancel(Kernel::HLERequestContext& ctx) {
        finished_event->Clear();
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_DEBUG(Service_NIM, "called");
    }

    void IsProcessing(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw<u32>(0); // We instantly process the request
        LOG_DEBUG(Service_NIM, "called");
    }

    void GetServerTime(Kernel::HLERequestContext& ctx) {
        const s64 server_time{std::chrono::duration_cast<std::chrono::seconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count()};
        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw<s64>(server_time);
        LOG_DEBUG(Service_NIM, "called");
    }
};

class NTC final : public ServiceFramework<NTC> {
public:
    explicit NTC() : ServiceFramework{"ntc"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &NTC::OpenEnsureNetworkClockAvailabilityService, "OpenEnsureNetworkClockAvailabilityService"},
            {100, &NTC::SuspendAutonomicTimeCorrection, "SuspendAutonomicTimeCorrection"},
            {101, &NTC::ResumeAutonomicTimeCorrection, "ResumeAutonomicTimeCorrection"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void OpenEnsureNetworkClockAvailabilityService(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IEnsureNetworkClockAvailabilityService>();
        LOG_DEBUG(Service_NIM, "called");
    }

    // TODO(ogniK): Do we need these?
    void SuspendAutonomicTimeCorrection(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_NIM, "(STUBBED) called");
    }

    void ResumeAutonomicTimeCorrection(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_NIM, "(STUBBED) called");
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<NIM>()->InstallAsService(sm);
    std::make_shared<NIM_SHP>()->InstallAsService(sm);
    std::make_shared<NTC>()->InstallAsService(sm);
}

} // namespace Service::NIM
