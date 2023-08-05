// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/olsc/olsc.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::OLSC {

class IOlscServiceForApplication final : public ServiceFramework<IOlscServiceForApplication> {
public:
    explicit IOlscServiceForApplication(Core::System& system_)
        : ServiceFramework{system_, "olsc:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IOlscServiceForApplication::Initialize, "Initialize"},
            {10, nullptr, "VerifySaveDataBackupLicenseAsync"},
            {13, &IOlscServiceForApplication::GetSaveDataBackupSetting, "GetSaveDataBackupSetting"},
            {14, &IOlscServiceForApplication::SetSaveDataBackupSettingEnabled, "SetSaveDataBackupSettingEnabled"},
            {15, nullptr, "SetCustomData"},
            {16, nullptr, "DeleteSaveDataBackupSetting"},
            {18, nullptr, "GetSaveDataBackupInfoCache"},
            {19, nullptr, "UpdateSaveDataBackupInfoCacheAsync"},
            {22, nullptr, "DeleteSaveDataBackupAsync"},
            {25, nullptr, "ListDownloadableSaveDataBackupInfoAsync"},
            {26, nullptr, "DownloadSaveDataBackupAsync"},
            {27, nullptr, "UploadSaveDataBackupAsync"},
            {9010, nullptr, "VerifySaveDataBackupLicenseAsyncForDebug"},
            {9013, nullptr, "GetSaveDataBackupSettingForDebug"},
            {9014, nullptr, "SetSaveDataBackupSettingEnabledForDebug"},
            {9015, nullptr, "SetCustomDataForDebug"},
            {9016, nullptr, "DeleteSaveDataBackupSettingForDebug"},
            {9018, nullptr, "GetSaveDataBackupInfoCacheForDebug"},
            {9019, nullptr, "UpdateSaveDataBackupInfoCacheAsyncForDebug"},
            {9022, nullptr, "DeleteSaveDataBackupAsyncForDebug"},
            {9025, nullptr, "ListDownloadableSaveDataBackupInfoAsyncForDebug"},
            {9026, nullptr, "DownloadSaveDataBackupAsyncForDebug"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void Initialize(HLERequestContext& ctx) {
        LOG_WARNING(Service_OLSC, "(STUBBED) called");

        initialized = true;

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetSaveDataBackupSetting(HLERequestContext& ctx) {
        LOG_WARNING(Service_OLSC, "(STUBBED) called");

        // backup_setting is set to 0 since real value is unknown
        constexpr u64 backup_setting = 0;

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(ResultSuccess);
        rb.Push(backup_setting);
    }

    void SetSaveDataBackupSettingEnabled(HLERequestContext& ctx) {
        LOG_WARNING(Service_OLSC, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    bool initialized{};
};

class INativeHandleHolder final : public ServiceFramework<INativeHandleHolder> {
public:
    explicit INativeHandleHolder(Core::System& system_)
        : ServiceFramework{system_, "INativeHandleHolder"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetNativeHandle"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class ITransferTaskListController final : public ServiceFramework<ITransferTaskListController> {
public:
    explicit ITransferTaskListController(Core::System& system_)
        : ServiceFramework{system_, "ITransferTaskListController"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown0"},
            {1, nullptr, "Unknown1"},
            {2, nullptr, "Unknown2"},
            {3, nullptr, "Unknown3"},
            {4, nullptr, "Unknown4"},
            {5, &ITransferTaskListController::GetNativeHandleHolder , "GetNativeHandleHolder"},
            {6, nullptr, "Unknown6"},
            {7, nullptr, "Unknown7"},
            {8, nullptr, "GetRemoteStorageController"},
            {9, &ITransferTaskListController::GetNativeHandleHolder, "GetNativeHandleHolder2"},
            {10, nullptr, "Unknown10"},
            {11, nullptr, "Unknown11"},
            {12, nullptr, "Unknown12"},
            {13, nullptr, "Unknown13"},
            {14, nullptr, "Unknown14"},
            {15, nullptr, "Unknown15"},
            {16, nullptr, "Unknown16"},
            {17, nullptr, "Unknown17"},
            {18, nullptr, "Unknown18"},
            {19, nullptr, "Unknown19"},
            {20, nullptr, "Unknown20"},
            {21, nullptr, "Unknown21"},
            {22, nullptr, "Unknown22"},
            {23, nullptr, "Unknown23"},
            {24, nullptr, "Unknown24"},
            {25, nullptr, "Unknown25"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetNativeHandleHolder(HLERequestContext& ctx) {
        LOG_INFO(Service_OLSC, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<INativeHandleHolder>(system);
    }
};

class IOlscServiceForSystemService final : public ServiceFramework<IOlscServiceForSystemService> {
public:
    explicit IOlscServiceForSystemService(Core::System& system_)
        : ServiceFramework{system_, "olsc:s"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IOlscServiceForSystemService::OpenTransferTaskListController, "OpenTransferTaskListController"},
            {1, nullptr, "OpenRemoteStorageController"},
            {2, nullptr, "OpenDaemonController"},
            {10, nullptr, "Unknown10"},
            {11, nullptr, "Unknown11"},
            {12, nullptr, "Unknown12"},
            {13, nullptr, "Unknown13"},
            {100, nullptr, "ListLastTransferTaskErrorInfo"},
            {101, nullptr, "GetLastErrorInfoCount"},
            {102, nullptr, "RemoveLastErrorInfoOld"},
            {103, nullptr, "GetLastErrorInfo"},
            {104, nullptr, "GetLastErrorEventHolder"},
            {105, nullptr, "GetLastTransferTaskErrorInfo"},
            {200, nullptr, "GetDataTransferPolicyInfo"},
            {201, nullptr, "RemoveDataTransferPolicyInfo"},
            {202, nullptr, "UpdateDataTransferPolicyOld"},
            {203, nullptr, "UpdateDataTransferPolicy"},
            {204, nullptr, "CleanupDataTransferPolicyInfo"},
            {205, nullptr, "RequestDataTransferPolicy"},
            {300, nullptr, "GetAutoTransferSeriesInfo"},
            {301, nullptr, "UpdateAutoTransferSeriesInfo"},
            {400, nullptr, "CleanupSaveDataArchiveInfoType1"},
            {900, nullptr, "CleanupTransferTask"},
            {902, nullptr, "CleanupSeriesInfoType0"},
            {903, nullptr, "CleanupSaveDataArchiveInfoType0"},
            {904, nullptr, "CleanupApplicationAutoTransferSetting"},
            {905, nullptr, "CleanupErrorHistory"},
            {906, nullptr, "SetLastError"},
            {907, nullptr, "AddSaveDataArchiveInfoType0"},
            {908, nullptr, "RemoveSeriesInfoType0"},
            {909, nullptr, "GetSeriesInfoType0"},
            {910, nullptr, "RemoveLastErrorInfo"},
            {911, nullptr, "CleanupSeriesInfoType1"},
            {912, nullptr, "RemoveSeriesInfoType1"},
            {913, nullptr, "GetSeriesInfoType1"},
            {1000, nullptr, "UpdateIssueOld"},
            {1010, nullptr, "Unknown1010"},
            {1011, nullptr, "ListIssueInfoOld"},
            {1012, nullptr, "GetIssueOld"},
            {1013, nullptr, "GetIssue2Old"},
            {1014, nullptr, "GetIssue3Old"},
            {1020, nullptr, "RepairIssueOld"},
            {1021, nullptr, "RepairIssueWithUserIdOld"},
            {1022, nullptr, "RepairIssue2Old"},
            {1023, nullptr, "RepairIssue3Old"},
            {1024, nullptr, "Unknown1024"},
            {1100, nullptr, "UpdateIssue"},
            {1110, nullptr, "Unknown1110"},
            {1111, nullptr, "ListIssueInfo"},
            {1112, nullptr, "GetIssue"},
            {1113, nullptr, "GetIssue2"},
            {1114, nullptr, "GetIssue3"},
            {1120, nullptr, "RepairIssue"},
            {1121, nullptr, "RepairIssueWithUserId"},
            {1122, nullptr, "RepairIssue2"},
            {1123, nullptr, "RepairIssue3"},
            {1124, nullptr, "Unknown1124"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void OpenTransferTaskListController(HLERequestContext& ctx) {
        LOG_INFO(Service_OLSC, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ITransferTaskListController>(system);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("olsc:u",
                                         std::make_shared<IOlscServiceForApplication>(system));
    server_manager->RegisterNamedService("olsc:s",
                                         std::make_shared<IOlscServiceForSystemService>(system));

    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::OLSC
