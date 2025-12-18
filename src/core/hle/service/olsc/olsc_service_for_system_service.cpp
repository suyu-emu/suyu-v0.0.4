// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/olsc/daemon_controller.h"
#include "core/hle/service/olsc/olsc_service_for_system_service.h"
#include "core/hle/service/olsc/remote_storage_controller.h"
#include "core/hle/service/olsc/transfer_task_list_controller.h"

namespace Service::OLSC {

IOlscServiceForSystemService::IOlscServiceForSystemService(Core::System& system_)
    : ServiceFramework{system_, "olsc:s"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, D<&IOlscServiceForSystemService::GetTransferTaskListController>, "GetTransferTaskListController"},
        {1, D<&IOlscServiceForSystemService::GetRemoteStorageController>, "GetRemoteStorageController"},
        {2, D<&IOlscServiceForSystemService::GetDaemonController>, "GetDaemonController"},
        {10, nullptr, "PrepareDeleteUserProperty"},
        {11, nullptr, "DeleteUserSaveDataProperty"},
        {12, nullptr, "InvalidateMountCache"},
        {13, nullptr, "DeleteDeviceSaveDataProperty"},
        {100, nullptr, "ListLastTransferTaskErrorInfo"},
        {101, nullptr, "GetLastErrorInfoCount"},
        {102, nullptr, "RemoveLastErrorInfoOld"},
        {103, nullptr, "GetLastErrorInfo"},
        {104, nullptr, "GetLastErrorEventHolder"},
        {105, nullptr, "GetLastTransferTaskErrorInfo"},
        {200, D<&IOlscServiceForSystemService::GetDataTransferPolicy>, "GetDataTransferPolicy"},
        {201, nullptr, "DeleteDataTransferPolicyCache"},
        {202, nullptr, "Unknown202"},
        {203, nullptr, "RequestUpdateDataTransferPolicyCacheAsync"},
        {204, nullptr, "ClearDataTransferPolicyCache"},
        {205, nullptr, "RequestGetDataTransferPolicyAsync"},
        {206, nullptr, "Unknown206"}, //21.0.0+
        {300, nullptr, "GetUserSaveDataProperty"},
        {301, nullptr, "SetUserSaveDataProperty"},
        {302, nullptr, "Unknown302"}, //21.0.0+
        {400, nullptr, "CleanupSaveDataBackupContextForSpecificApplications"},
        {900, nullptr, "DeleteAllTransferTask"},
        {902, nullptr, "DeleteAllSeriesInfo"},
        {903, nullptr, "DeleteAllSdaInfoCache"},
        {904, nullptr, "DeleteAllApplicationSetting"},
        {905, nullptr, "DeleteAllTransferTaskErrorInfo"},
        {906, nullptr, "RegisterTransferTaskErrorInfo"},
        {907, nullptr, "AddSaveDataArchiveInfoCache"},
        {908, nullptr, "DeleteSeriesInfo"},
        {909, nullptr, "GetSeriesInfo"},
        {910, nullptr, "RemoveTransferTaskErrorInfo"},
        {911, nullptr, "DeleteAllSeriesInfoForSaveDataBackup"},
        {912, nullptr, "DeleteSeriesInfoForSaveDataBackup"},
        {913, nullptr, "GetSeriesInfoForSaveDataBackup"},
        {914, nullptr, "Unknown914"}, //20.2.0+
        {1000, nullptr, "UpdateIssueOld"},
        {1010, nullptr, "Unknown1010"},
        {1011, nullptr, "Unknown1011"},
        {1012, nullptr, "Unknown1012"},
        {1013, nullptr, "Unkown1013"},
        {1014, nullptr, "Unknown1014"},
        {1020, nullptr, "Unknown1020"},
        {1021, nullptr, "Unknown1021"},
        {1022, nullptr, "Unknown1022"},
        {1023, nullptr, "Unknown1023"},
        {1024, nullptr, "Unknown1024"},
        {1100, nullptr, "RepairUpdateIssueInfoCacheAync"},
        {1110, nullptr, "RepairGetIssueInfo"},
        {1111, nullptr, "RepairListIssueInfo"},
        {1112, nullptr, "RepairListOperationPermissionInfo"},
        {1113, nullptr, "RepairListDataInfoForRepairedSaveDataDownload"},
        {1114, nullptr, "RepairListDataInfoForOriginalSaveDataDownload"},
        {1120, nullptr, "RepairUploadSaveDataAsync"},
        {1121, nullptr, "RepairUploadSaveDataAsync1"},
        {1122, nullptr, "RepairDownloadRepairedSaveDataAsync"},
        {1123, nullptr, "RepairDownloadOriginalSaveDataAsync"},
        {1124, nullptr, "RepairGetOperationProgressInfo"},
        {10000, D<&IOlscServiceForSystemService::GetOlscServiceForSystemService>, "GetOlscServiceForSystemService"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IOlscServiceForSystemService::~IOlscServiceForSystemService() = default;

Result IOlscServiceForSystemService::GetTransferTaskListController(
    Out<SharedPointer<ITransferTaskListController>> out_interface) {
    LOG_INFO(Service_OLSC, "called");
    *out_interface = std::make_shared<ITransferTaskListController>(system);
    R_SUCCEED();
}

Result IOlscServiceForSystemService::GetRemoteStorageController(
    Out<SharedPointer<IRemoteStorageController>> out_interface) {
    LOG_INFO(Service_OLSC, "called");
    *out_interface = std::make_shared<IRemoteStorageController>(system);
    R_SUCCEED();
}

Result IOlscServiceForSystemService::GetDaemonController(
    Out<SharedPointer<IDaemonController>> out_interface) {
    LOG_INFO(Service_OLSC, "called");
    *out_interface = std::make_shared<IDaemonController>(system);
    R_SUCCEED();
}

Result IOlscServiceForSystemService::GetDataTransferPolicy(
    Out<DataTransferPolicy> out_policy, u64 application_id) {
    LOG_WARNING(Service_OLSC, "(STUBBED) called");
    DataTransferPolicy policy{};
    policy.upload_policy = 0;
    policy.download_policy = 0;
    *out_policy = policy;
    R_SUCCEED();
}

Result IOlscServiceForSystemService::GetOlscServiceForSystemService(
    Out<SharedPointer<IOlscServiceForSystemService>> out_interface) {
    LOG_INFO(Service_OLSC, "called");
    *out_interface = std::static_pointer_cast<IOlscServiceForSystemService>(shared_from_this());
    R_SUCCEED();
}

} // namespace Service::OLSC
