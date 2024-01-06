// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <chrono>
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/set/settings_server.h"

namespace Service::Set {
namespace {
constexpr std::size_t PRE_4_0_0_MAX_ENTRIES = 0xF;
constexpr std::size_t POST_4_0_0_MAX_ENTRIES = 0x40;

constexpr Result ResultInvalidLanguage{ErrorModule::Settings, 625};

void PushResponseLanguageCode(HLERequestContext& ctx, std::size_t num_language_codes) {
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(num_language_codes));
}

void GetAvailableLanguageCodesImpl(HLERequestContext& ctx, std::size_t max_entries) {
    const std::size_t requested_amount = ctx.GetWriteBufferNumElements<LanguageCode>();
    const std::size_t max_amount = std::min(requested_amount, max_entries);
    const std::size_t copy_amount = std::min(available_language_codes.size(), max_amount);
    const std::size_t copy_size = copy_amount * sizeof(LanguageCode);

    ctx.WriteBuffer(available_language_codes.data(), copy_size);
    PushResponseLanguageCode(ctx, copy_amount);
}

void GetKeyCodeMapImpl(HLERequestContext& ctx) {
    const auto language_code =
        available_language_codes[static_cast<s32>(Settings::values.language_index.GetValue())];
    const auto key_code =
        std::find_if(language_to_layout.cbegin(), language_to_layout.cend(),
                     [=](const auto& element) { return element.first == language_code; });
    KeyboardLayout layout = KeyboardLayout::EnglishUs;
    if (key_code == language_to_layout.cend()) {
        LOG_ERROR(Service_SET,
                  "Could not find keyboard layout for language index {}, defaulting to English us",
                  Settings::values.language_index.GetValue());
    } else {
        layout = key_code->second;
    }

    ctx.WriteBuffer(layout);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}
} // Anonymous namespace

LanguageCode GetLanguageCodeFromIndex(std::size_t index) {
    return available_language_codes.at(index);
}

ISettingsServer::ISettingsServer(Core::System& system_) : ServiceFramework{system_, "set"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &ISettingsServer::GetLanguageCode, "GetLanguageCode"},
        {1, &ISettingsServer::GetAvailableLanguageCodes, "GetAvailableLanguageCodes"},
        {2, &ISettingsServer::MakeLanguageCode, "MakeLanguageCode"},
        {3, &ISettingsServer::GetAvailableLanguageCodeCount, "GetAvailableLanguageCodeCount"},
        {4, &ISettingsServer::GetRegionCode, "GetRegionCode"},
        {5, &ISettingsServer::GetAvailableLanguageCodes2, "GetAvailableLanguageCodes2"},
        {6, &ISettingsServer::GetAvailableLanguageCodeCount2, "GetAvailableLanguageCodeCount2"},
        {7, &ISettingsServer::GetKeyCodeMap, "GetKeyCodeMap"},
        {8, &ISettingsServer::GetQuestFlag, "GetQuestFlag"},
        {9, &ISettingsServer::GetKeyCodeMap2, "GetKeyCodeMap2"},
        {10, nullptr, "GetFirmwareVersionForDebug"},
        {11, &ISettingsServer::GetDeviceNickName, "GetDeviceNickName"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

ISettingsServer::~ISettingsServer() = default;

void ISettingsServer::GetAvailableLanguageCodes(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    GetAvailableLanguageCodesImpl(ctx, PRE_4_0_0_MAX_ENTRIES);
}

void ISettingsServer::MakeLanguageCode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto index = rp.Pop<u32>();

    if (index >= available_language_codes.size()) {
        LOG_ERROR(Service_SET, "Invalid language code index! index={}", index);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(Set::ResultInvalidLanguage);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.PushEnum(available_language_codes[index]);
}

void ISettingsServer::GetAvailableLanguageCodes2(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    GetAvailableLanguageCodesImpl(ctx, POST_4_0_0_MAX_ENTRIES);
}

void ISettingsServer::GetAvailableLanguageCodeCount(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    PushResponseLanguageCode(ctx, PRE_4_0_0_MAX_ENTRIES);
}

void ISettingsServer::GetAvailableLanguageCodeCount2(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    PushResponseLanguageCode(ctx, POST_4_0_0_MAX_ENTRIES);
}

void ISettingsServer::GetQuestFlag(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<s32>(Settings::values.quest_flag.GetValue()));
}

void ISettingsServer::GetLanguageCode(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called {}", Settings::values.language_index.GetValue());

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.PushEnum(
        available_language_codes[static_cast<s32>(Settings::values.language_index.GetValue())]);
}

void ISettingsServer::GetRegionCode(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(Settings::values.region_index.GetValue()));
}

void ISettingsServer::GetKeyCodeMap(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "Called {}", ctx.Description());
    GetKeyCodeMapImpl(ctx);
}

void ISettingsServer::GetKeyCodeMap2(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "Called {}", ctx.Description());
    GetKeyCodeMapImpl(ctx);
}

void ISettingsServer::GetDeviceNickName(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
    ctx.WriteBuffer(Settings::values.device_name.GetValue());
}

} // namespace Service::Set
