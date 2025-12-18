// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/bcat/news/news_service.h"
#include "core/hle/service/cmif_serialization.h"

namespace Service::News {

INewsService::INewsService(Core::System& system_) : ServiceFramework{system_, "INewsService"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {10100, D<&INewsService::PostLocalNews>, "PostLocalNews"},
        {20100, nullptr, "SetPassphrase"},
        {30100, D<&INewsService::GetSubscriptionStatus>, "GetSubscriptionStatus"},
        {30101, nullptr, "GetTopicList"}, //3.0.0+
        {30110, nullptr, "Unknown30110"}, //6.0.0+
        {30200, D<&INewsService::IsSystemUpdateRequired>, "IsSystemUpdateRequired"},
        {30201, nullptr, "Unknown30201"}, //8.0.0+
        {30210, nullptr, "Unknown30210"}, //10.0.0+
        {30300, nullptr, "RequestImmediateReception"},
        {30400, nullptr, "DecodeArchiveFile"}, //3.0.0-18.1.0
        {30500, nullptr, "Unknown30500"}, //8.0.0+
        {30900, nullptr, "Unknown30900"}, //1.0.0
        {30901, nullptr, "Unknown30901"}, //1.0.0
        {30902, nullptr, "Unknown30902"}, //1.0.0
        {40100, nullptr, "SetSubscriptionStatus"},
        {40101, D<&INewsService::RequestAutoSubscription>, "RequestAutoSubscription"}, //3.0.0+
        {40200, nullptr, "ClearStorage"},
        {40201, nullptr, "ClearSubscriptionStatusAll"},
        {90100, nullptr, "GetNewsDatabaseDump"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

INewsService::~INewsService() = default;

Result INewsService::PostLocalNews(InBuffer<BufferAttr_HipcAutoSelect> buffer_data) {
    LOG_WARNING(Service_BCAT, "(STUBBED) called, buffer_size={}", buffer_data.size());

    R_SUCCEED();
}

Result INewsService::GetSubscriptionStatus(Out<u32> out_status,
                                           InBuffer<BufferAttr_HipcPointer> buffer_data) {
    LOG_WARNING(Service_BCAT, "(STUBBED) called, buffer_size={}", buffer_data.size());
    *out_status = 0;
    R_SUCCEED();
}

Result INewsService::IsSystemUpdateRequired(Out<bool> out_is_system_update_required) {
    LOG_WARNING(Service_BCAT, "(STUBBED) called");
    *out_is_system_update_required = false;
    R_SUCCEED();
}

Result INewsService::RequestAutoSubscription(u64 value) {
    LOG_WARNING(Service_BCAT, "(STUBBED) called");
    R_SUCCEED();
}

} // namespace Service::News
