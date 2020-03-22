// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <chrono>
#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/set/set.h"
#include "core/settings.h"

namespace Service::Set {
namespace {
constexpr std::array<LanguageCode, 17> available_language_codes = {{
    LanguageCode::JA,
    LanguageCode::EN_US,
    LanguageCode::FR,
    LanguageCode::DE,
    LanguageCode::IT,
    LanguageCode::ES,
    LanguageCode::ZH_CN,
    LanguageCode::KO,
    LanguageCode::NL,
    LanguageCode::PT,
    LanguageCode::RU,
    LanguageCode::ZH_TW,
    LanguageCode::EN_GB,
    LanguageCode::FR_CA,
    LanguageCode::ES_419,
    LanguageCode::ZH_HANS,
    LanguageCode::ZH_HANT,
}};

constexpr std::size_t pre4_0_0_max_entries = 15;
constexpr std::size_t post4_0_0_max_entries = 17;

constexpr ResultCode ERR_INVALID_LANGUAGE{ErrorModule::Settings, 625};

void PushResponseLanguageCode(Kernel::HLERequestContext& ctx, std::size_t num_language_codes) {
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.Push(static_cast<u32>(num_language_codes));
}

void GetAvailableLanguageCodesImpl(Kernel::HLERequestContext& ctx, std::size_t max_size) {
    const std::size_t requested_amount = ctx.GetWriteBufferSize() / sizeof(LanguageCode);
    const std::size_t copy_amount = std::min(requested_amount, max_size);
    const std::size_t copy_size = copy_amount * sizeof(LanguageCode);

    ctx.WriteBuffer(available_language_codes.data(), copy_size);
    PushResponseLanguageCode(ctx, copy_amount);
}
} // Anonymous namespace

LanguageCode GetLanguageCodeFromIndex(std::size_t index) {
    return available_language_codes.at(index);
}

void SET::GetAvailableLanguageCodes(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    GetAvailableLanguageCodesImpl(ctx, pre4_0_0_max_entries);
}

void SET::MakeLanguageCode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto index = rp.Pop<u32>();

    if (index >= available_language_codes.size()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ERR_INVALID_LANGUAGE);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(available_language_codes[index]);
}

void SET::GetAvailableLanguageCodes2(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    GetAvailableLanguageCodesImpl(ctx, post4_0_0_max_entries);
}

void SET::GetAvailableLanguageCodeCount(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    PushResponseLanguageCode(ctx, pre4_0_0_max_entries);
}

void SET::GetAvailableLanguageCodeCount2(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    PushResponseLanguageCode(ctx, post4_0_0_max_entries);
}

void SET::GetQuestFlag(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.Push(static_cast<u32>(Settings::values.quest_flag));
}

void SET::GetLanguageCode(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called {}", Settings::values.language_index);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(available_language_codes[Settings::values.language_index]);
}

void SET::GetRegionCode(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.Push(Settings::values.region_index);
}

SET::SET() : ServiceFramework("set") {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &SET::GetLanguageCode, "GetLanguageCode"},
        {1, &SET::GetAvailableLanguageCodes, "GetAvailableLanguageCodes"},
        {2, &SET::MakeLanguageCode, "MakeLanguageCode"},
        {3, &SET::GetAvailableLanguageCodeCount, "GetAvailableLanguageCodeCount"},
        {4, &SET::GetRegionCode, "GetRegionCode"},
        {5, &SET::GetAvailableLanguageCodes2, "GetAvailableLanguageCodes2"},
        {6, &SET::GetAvailableLanguageCodeCount2, "GetAvailableLanguageCodeCount2"},
        {7, nullptr, "GetKeyCodeMap"},
        {8, &SET::GetQuestFlag, "GetQuestFlag"},
        {9, nullptr, "GetKeyCodeMap2"},
        {10, nullptr, "GetFirmwareVersionForDebug"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

SET::~SET() = default;

} // namespace Service::Set
