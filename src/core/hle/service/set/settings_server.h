// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"
#include "core/hle/service/set/settings_types.h"

namespace Core {
class System;
}

namespace Service::Set {

LanguageCode GetLanguageCodeFromIndex(std::size_t idx);

class ISettingsServer final : public ServiceFramework<ISettingsServer> {
public:
    explicit ISettingsServer(Core::System& system_);
    ~ISettingsServer() override;

private:
    void GetLanguageCode(HLERequestContext& ctx);
    void GetAvailableLanguageCodes(HLERequestContext& ctx);
    void MakeLanguageCode(HLERequestContext& ctx);
    void GetAvailableLanguageCodes2(HLERequestContext& ctx);
    void GetAvailableLanguageCodeCount(HLERequestContext& ctx);
    void GetAvailableLanguageCodeCount2(HLERequestContext& ctx);
    void GetQuestFlag(HLERequestContext& ctx);
    void GetRegionCode(HLERequestContext& ctx);
    void GetKeyCodeMap(HLERequestContext& ctx);
    void GetKeyCodeMap2(HLERequestContext& ctx);
    void GetDeviceNickName(HLERequestContext& ctx);
};

} // namespace Service::Set
