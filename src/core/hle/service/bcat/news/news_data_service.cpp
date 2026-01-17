// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/bcat/news/news_data_service.h"
#include "core/hle/service/bcat/news/builtin_news.h"
#include "core/hle/service/bcat/news/news_storage.h"
#include "core/hle/service/cmif_serialization.h"

#include "common/logging/log.h"

#include <cstring>

namespace Service::News {
namespace {

std::string_view ToStringView(std::span<const char> buf) {
    const std::string_view sv{buf.data(), buf.size()};
    const auto nul = sv.find('\0');
    return nul == std::string_view::npos ? sv : sv.substr(0, nul);
}

} // namespace

INewsDataService::INewsDataService(Core::System& system_)
    : ServiceFramework{system_, "INewsDataService"} {
    static const FunctionInfo functions[] = {
        {0, D<&INewsDataService::Open>, "Open"},
        {1, D<&INewsDataService::OpenWithNewsRecordV1>, "OpenWithNewsRecordV1"},
        {2, D<&INewsDataService::Read>, "Read"},
        {3, D<&INewsDataService::GetSize>, "GetSize"},
        {1001, D<&INewsDataService::OpenWithNewsRecord>, "OpenWithNewsRecord"},
    };
    RegisterHandlers(functions);
}

INewsDataService::~INewsDataService() = default;

bool INewsDataService::TryOpen(std::string_view key, std::string_view user) {
    opened_payload.clear();

    if (auto found = NewsStorage::Instance().FindByNewsId(key, user)) {
        opened_payload = std::move(found->payload);
        return true;
    }

    if (!user.empty()) {
        if (auto found = NewsStorage::Instance().FindByNewsId(key)) {
            opened_payload = std::move(found->payload);
            return true;
        }
    }

    const auto list = NewsStorage::Instance().ListAll();
    if (!list.empty()) {
        if (auto found = NewsStorage::Instance().FindByNewsId(ToStringView(list.front().news_id))) {
            opened_payload = std::move(found->payload);
            return true;
        }
    }

    return false;
}

Result INewsDataService::Open(InBuffer<BufferAttr_HipcMapAlias> name) {
    EnsureBuiltinNewsLoaded();

    const auto key = ToStringView({reinterpret_cast<const char*>(name.data()), name.size()});

    if (TryOpen(key, {})) {
        R_SUCCEED();
    }

    R_RETURN(ResultUnknown);
}

Result INewsDataService::OpenWithNewsRecordV1(NewsRecordV1 record) {
    EnsureBuiltinNewsLoaded();

    const auto key = ToStringView(record.news_id);
    const auto user = ToStringView(record.user_id);

    if (TryOpen(key, user)) {
        R_SUCCEED();
    }

    R_RETURN(ResultUnknown);
}

Result INewsDataService::OpenWithNewsRecord(NewsRecord record) {
    EnsureBuiltinNewsLoaded();

    const auto key = ToStringView(record.news_id);
    const auto user = ToStringView(record.user_id);

    if (TryOpen(key, user)) {
        R_SUCCEED();
    }

    R_RETURN(ResultUnknown);
}

Result INewsDataService::Read(Out<u64> out_size, s64 offset,
                              OutBuffer<BufferAttr_HipcMapAlias> out_buffer) {
    const auto off = static_cast<size_t>(std::max<s64>(0, offset));

    if (off >= opened_payload.size()) {
        *out_size = 0;
        R_SUCCEED();
    }

    const size_t len = std::min(out_buffer.size(), opened_payload.size() - off);
    std::memcpy(out_buffer.data(), opened_payload.data() + off, len);
    *out_size = len;
    R_SUCCEED();
}

Result INewsDataService::GetSize(Out<s64> out_size) {
    *out_size = static_cast<s64>(opened_payload.size());
    R_SUCCEED();
}

} // namespace Service::News
