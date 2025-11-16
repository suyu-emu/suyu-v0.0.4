// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/olsc/remote_storage_controller.h"

namespace Service::OLSC {

IRemoteStorageController::IRemoteStorageController(Core::System& system_)
    : ServiceFramework{system_, "IRemoteStorageController"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "GetSaveDataArchiveInfoBySaveDataId"},
        {1, nullptr, "GetSaveDataArchiveInfoByApplicationId"},
        {3, nullptr, "GetSaveDataArchiveCount"},
        {6, nullptr, "CleanupSaveDataArchives"},
        {7, nullptr, "CreateSaveDataArchiveCacheUpdationTask"},
        {8, nullptr, "CreateSaveDataArchiveCacheUpdationForSpecifiedApplicationTask"},
        {9, nullptr, "Delete"},
        {10, nullptr, "GetSeriesInfo"},
        {11, nullptr, "CreateDeleteDataTask"},
        {12, nullptr, "DeleteSeriesInfo"},
        {13, nullptr, "CreateRegisterNotificationTokenTask"},
        {14, nullptr, "UpdateSeriesInfo"},
        {15, nullptr, "RegisterUploadSaveDataTransferTaskForAutonomyRegistration"},
        {16, nullptr, "CreateCleanupToDeleteSaveDataArchiveInfoTask"},
        {17, nullptr, "ListDataInfo"},
        {18, nullptr, "GetDataInfo"},
        {19, nullptr, "GetDataInfoCacheUpdateNativeHandleHolder"},
        {20, nullptr, "CreateSaveDataArchiveInfoCacheForSaveDataBackupUpdationTask"},
        {21, nullptr, "ListSecondarySaves"},
        {22, D<&IRemoteStorageController::GetSecondarySave>, "GetSecondarySave"},
        {23, nullptr, "TouchSecondarySave"},
        {24, nullptr, "GetSecondarySaveDataInfo"},
        {25, nullptr, "RegisterDownloadSaveDataTransferTaskForAutonomyRegistration"},
        {26, nullptr, "Unknown26"}, //20.0.0+
        {27, nullptr, "Unknown27"}, //20.0.0+
        {28, nullptr, "Unknown28"}, //20.0.0+
        {29, nullptr, "Unknown29"}, //21.0.0+
        {800, nullptr, "Unknown800"}, //20.0.0+
        {900, nullptr, "SetLoadedDataMissing"},
        {901, nullptr, "Unknown901"}, //20.2.0+
    };
    // clang-format on

    RegisterHandlers(functions);
}

IRemoteStorageController::~IRemoteStorageController() = default;

Result IRemoteStorageController::GetSecondarySave(Out<bool> out_has_secondary_save,
                                                  Out<std::array<u64, 3>> out_unknown,
                                                  u64 application_id) {
    LOG_ERROR(Service_OLSC, "(STUBBED) called, application_id={:016X}", application_id);
    *out_has_secondary_save = false;
    *out_unknown = {};
    R_SUCCEED();
}

} // namespace Service::OLSC
