// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/service.h"

namespace Service::OLSC {

class IOlscServiceForApplication final : public ServiceFramework<IOlscServiceForApplication> {
public:
    explicit IOlscServiceForApplication(Core::System& system_);
    ~IOlscServiceForApplication() override;

private:
    void Initialize(HLERequestContext& ctx);
    void GetSaveDataBackupSetting(HLERequestContext& ctx);
    void SetSaveDataBackupSettingEnabled(HLERequestContext& ctx);

    bool initialized{};
};

} // namespace Service::OLSC
