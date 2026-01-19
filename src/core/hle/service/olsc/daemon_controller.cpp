// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/olsc/daemon_controller.h"

namespace Service::OLSC {

IDaemonController::IDaemonController(Core::System& system_)
    : ServiceFramework{system_, "IDaemonController"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, D<&IDaemonController::GetApplicationAutoTransferSetting>, "GetApplicationAutoTransferSetting"},
        {1, D<&IDaemonController::SetApplicationAutoTransferSetting>, "SetApplicationAutoTransferSetting"},
        {2, D<&IDaemonController::GetGlobalAutoUploadSetting>, "GetGlobalAutoUploadSetting"},
        {3, D<&IDaemonController::SetGlobalAutoUploadSetting>, "SetGlobalAutoUploadSetting"},
        {4, D<&IDaemonController::RunTransferTaskAutonomyRegistration>, "RunTransferTaskAutonomyRegistration"},
        {5, D<&IDaemonController::GetGlobalAutoDownloadSetting>, "GetGlobalAutoDownloadSetting"}, // 11.0.0+
        {6, D<&IDaemonController::SetGlobalAutoDownloadSetting>, "SetGlobalAutoDownloadSetting"}, // 11.0.0+
        {10, nullptr, "CreateForbiddenSaveDataInidication"},
        {11, nullptr, "StopAutonomyTaskExecution"},
        {12, D<&IDaemonController::GetAutonomyTaskStatus>, "GetAutonomyTaskStatus"},
        {13, nullptr, "Unknown13_20_0_0_Plus"}, // 20.0.0+
    };
    // clang-format on

    RegisterHandlers(functions);
}

IDaemonController::~IDaemonController() = default;

Result IDaemonController::GetApplicationAutoTransferSetting(Out<bool> out_is_enabled,
                                                            Common::UUID user_id,
                                                            u64 application_id) {
    LOG_INFO(Service_OLSC, "called, user_id={} application_id={:016X}", user_id.FormattedString(),
             application_id);
    AppKey key{user_id, application_id};
    const auto it = app_auto_transfer_.find(key);
    *out_is_enabled = (it != app_auto_transfer_.end()) ? it->second : false;
    R_SUCCEED();
}

Result IDaemonController::SetApplicationAutoTransferSetting(bool is_enabled, Common::UUID user_id,
                                                            u64 application_id) {
    LOG_INFO(Service_OLSC, "called, user_id={} application_id={:016X} is_enabled={}",
             user_id.FormattedString(), application_id, is_enabled);
    AppKey key{user_id, application_id};
    app_auto_transfer_[key] = is_enabled;
    R_SUCCEED();
}

Result IDaemonController::GetGlobalAutoUploadSetting(Out<bool> out_is_enabled,
                                                     Common::UUID user_id) {
    LOG_INFO(Service_OLSC, "called, user_id={}", user_id.FormattedString());
    const auto it = global_auto_upload_.find(user_id);
    *out_is_enabled = (it != global_auto_upload_.end()) ? it->second : false;
    R_SUCCEED();
}

Result IDaemonController::SetGlobalAutoUploadSetting(bool is_enabled, Common::UUID user_id) {
    LOG_INFO(Service_OLSC, "called, user_id={} is_enabled={}", user_id.FormattedString(), is_enabled);
    global_auto_upload_[user_id] = is_enabled;
    R_SUCCEED();
}

Result IDaemonController::RunTransferTaskAutonomyRegistration(Common::UUID user_id,
                                                              u64 application_id) {
    LOG_INFO(Service_OLSC, "called, user_id={} application_id={:016X}", user_id.FormattedString(),
             application_id);
    // Simulate starting an autonomy task: set status to 1 (running) then back to 0 (idle)
    AppKey key{user_id, application_id};
    autonomy_task_status_[key] = 1; // running
    // TODO: In a real implementation this would be asynchronous. We'll immediately set to idle.
    autonomy_task_status_[key] = 0; // idle
    R_SUCCEED();
}

Result IDaemonController::GetGlobalAutoDownloadSetting(Out<bool> out_is_enabled,
                                                       Common::UUID user_id) {
    LOG_INFO(Service_OLSC, "called, user_id={}", user_id.FormattedString());
    const auto it = global_auto_download_.find(user_id);
    *out_is_enabled = (it != global_auto_download_.end()) ? it->second : false;
    R_SUCCEED();
}

Result IDaemonController::SetGlobalAutoDownloadSetting(bool is_enabled, Common::UUID user_id) {
    LOG_INFO(Service_OLSC, "called, user_id={} is_enabled={}", user_id.FormattedString(), is_enabled);
    global_auto_download_[user_id] = is_enabled;
    R_SUCCEED();
}

Result IDaemonController::GetAutonomyTaskStatus(Out<u8> out_status, Common::UUID user_id) {
    LOG_INFO(Service_OLSC, "called, user_id={}", user_id.FormattedString());

    *out_status = 0;
    R_SUCCEED();
}

} // namespace Service::OLSC
