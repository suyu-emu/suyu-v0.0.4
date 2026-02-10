// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <ankerl/unordered_dense.h>

#include "common/uuid.h"
#include "core/hle/service/cmif_types.h"
#include "core/hle/service/olsc/stopper_object.h"
#include "core/hle/service/service.h"

namespace Service::OLSC {

class IDaemonController final : public ServiceFramework<IDaemonController> {
public:
    explicit IDaemonController(Core::System& system_);
    ~IDaemonController() override;

private:
    Result GetApplicationAutoTransferSetting(Out<bool> out_is_enabled, Common::UUID user_id,
                                             u64 application_id);
    Result SetApplicationAutoTransferSetting(bool is_enabled, Common::UUID user_id,
                                             u64 application_id);

    Result GetGlobalAutoUploadSetting(Out<bool> out_is_enabled, Common::UUID user_id);
    Result SetGlobalAutoUploadSetting(bool is_enabled, Common::UUID user_id);

    Result RunTransferTaskAutonomyRegistration(Common::UUID user_id, u64 application_id);

    Result GetGlobalAutoDownloadSetting(Out<bool> out_is_enabled, Common::UUID user_id);
    Result SetGlobalAutoDownloadSetting(bool is_enabled, Common::UUID user_id);

    Result StopAutonomyTaskExecution(Out<SharedPointer<IStopperObject>> out_stopper);

    Result GetAutonomyTaskStatus(Out<u8> out_status, Common::UUID user_id);

    // Internal in-memory state to back the above APIs
    struct AppKey {
        Common::UUID user_id;
        u64 application_id{};
        friend constexpr bool operator==(const AppKey& a, const AppKey& b) {
            return a.user_id == b.user_id && a.application_id == b.application_id;
        }
    };
    struct AppKeyHash {
        size_t operator()(const AppKey& k) const noexcept {
            // Combine UUID hash and application_id
            size_t h1 = std::hash<Common::UUID>{}(k.user_id);
            size_t h2 = std::hash<u64>{}(k.application_id);
            // Simple hash combine
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
        }
    };

    ankerl::unordered_dense::map<AppKey, bool, AppKeyHash> app_auto_transfer_{};
    ankerl::unordered_dense::map<Common::UUID, bool> global_auto_upload_{};
    ankerl::unordered_dense::map<Common::UUID, bool> global_auto_download_{};
    ankerl::unordered_dense::map<AppKey, u8, AppKeyHash> autonomy_task_status_{};
};

} // namespace Service::OLSC
