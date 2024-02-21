// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/olsc/olsc_service_for_system_service.h"
#include "core/hle/service/olsc/transfer_task_list_controller.h"

namespace Service::OLSC {

IOlscServiceForSystemService::IOlscServiceForSystemService(Core::System& system_)
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

IOlscServiceForSystemService::~IOlscServiceForSystemService() = default;

void IOlscServiceForSystemService::OpenTransferTaskListController(HLERequestContext& ctx) {
    LOG_INFO(Service_OLSC, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ITransferTaskListController>(system);
}

} // namespace Service::OLSC
