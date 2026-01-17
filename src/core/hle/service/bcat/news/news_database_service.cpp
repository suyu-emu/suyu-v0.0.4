// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/bcat/news/news_database_service.h"
#include "core/hle/service/bcat/news/builtin_news.h"
#include "core/hle/service/bcat/news/news_storage.h"
#include "core/hle/service/cmif_serialization.h"

#include <algorithm>
#include <cstring>

namespace Service::News {
namespace {

std::string_view ToStringView(std::span<const u8> buf) {
    if (buf.empty()) return {};
    auto data = reinterpret_cast<const char*>(buf.data());
    return {data, strnlen(data, buf.size())};
}

std::string_view ToStringView(std::span<const char> buf) {
    if (buf.empty()) return {};
    return {buf.data(), strnlen(buf.data(), buf.size())};
}

bool UpdateField(NewsRecord& rec, std::string_view column, s32 value, bool additive) {
    auto apply = [&](s32& field) {
        field = additive ? field + value : value;
        return true;
    };

    if (column == "read") return apply(rec.read);
    if (column == "newly") return apply(rec.newly);
    if (column == "displayed") return apply(rec.displayed);
    if (column == "extra1" || column == "extra_1") return apply(rec.extra1);
    if (column == "extra2" || column == "extra_2") return apply(rec.extra2);

    // Accept but ignore fields that don't exist in our struct
    return column == "priority" || column == "decoration_type" ||
           column == "feedback" || column == "category";
}

} // namespace

INewsDatabaseService::INewsDatabaseService(Core::System& system_)
    : ServiceFramework{system_, "INewsDatabaseService"} {
    static const FunctionInfo functions[] = {
        {0, D<&INewsDatabaseService::GetListV1>, "GetListV1"},
        {1, D<&INewsDatabaseService::Count>, "Count"},
        {2, D<&INewsDatabaseService::CountWithKey>, "CountWithKey"},
        {3, D<&INewsDatabaseService::UpdateIntegerValue>, "UpdateIntegerValue"},
        {4, D<&INewsDatabaseService::UpdateIntegerValueWithAddition>, "UpdateIntegerValueWithAddition"},
        {5, D<&INewsDatabaseService::UpdateStringValue>, "UpdateStringValue"},
        {1000, D<&INewsDatabaseService::GetList>, "GetList"},
    };
    RegisterHandlers(functions);
}

INewsDatabaseService::~INewsDatabaseService() = default;

Result INewsDatabaseService::Count(Out<s32> out_count, InBuffer<BufferAttr_HipcPointer> where) {
    EnsureBuiltinNewsLoaded();
    *out_count = static_cast<s32>(NewsStorage::Instance().ListAll().size());
    R_SUCCEED();
}

Result INewsDatabaseService::CountWithKey(Out<s32> out_count,
                                          InBuffer<BufferAttr_HipcPointer> key,
                                          InBuffer<BufferAttr_HipcPointer> where) {
    EnsureBuiltinNewsLoaded();
    *out_count = static_cast<s32>(NewsStorage::Instance().ListAll().size());
    R_SUCCEED();
}

Result INewsDatabaseService::UpdateIntegerValue(u32 value,
                                                InBuffer<BufferAttr_HipcPointer> key,
                                                InBuffer<BufferAttr_HipcPointer> where) {
    const auto column = ToStringView(key);
    for (const auto& rec : NewsStorage::Instance().ListAll()) {
        NewsStorage::Instance().UpdateRecord(
            ToStringView(rec.news_id), {},
            [&](NewsRecord& r) { UpdateField(r, column, static_cast<s32>(value), false); });
    }
    R_SUCCEED();
}

Result INewsDatabaseService::UpdateIntegerValueWithAddition(u32 value,
                                                            InBuffer<BufferAttr_HipcPointer> key,
                                                            InBuffer<BufferAttr_HipcPointer> where) {
    const auto column = ToStringView(key);
    const auto where_str = ToStringView(where);

    // Extract news_id from where clause like "N_SWITCH(news_id,'LA00000000000123456',1,0)=1"
    auto extract_news_id = [](std::string_view w) -> std::string {
        auto pos = w.find("'LA");
        if (pos == std::string_view::npos) return {};
        pos++; // skip the '
        auto end = w.find("'", pos);
        if (end == std::string_view::npos) return {};
        return std::string(w.substr(pos, end - pos));
    };

    const auto news_id = extract_news_id(where_str);

    if (column == "read" && value > 0 && !news_id.empty()) {
        NewsStorage::Instance().MarkAsRead(news_id);
    } else if (!news_id.empty()) {
        NewsStorage::Instance().UpdateRecord(news_id, {},
            [&](NewsRecord& r) { UpdateField(r, column, static_cast<s32>(value), true); });
    }

    R_SUCCEED();
}

Result INewsDatabaseService::UpdateStringValue(InBuffer<BufferAttr_HipcPointer> key,
                                               InBuffer<BufferAttr_HipcPointer> value,
                                               InBuffer<BufferAttr_HipcPointer> where) {
    LOG_WARNING(Service_BCAT, "(STUBBED) UpdateStringValue");
    R_SUCCEED();
}

Result INewsDatabaseService::GetListV1(Out<s32> out_count,
                                       OutBuffer<BufferAttr_HipcMapAlias> out_buffer,
                                       InBuffer<BufferAttr_HipcPointer> where,
                                       InBuffer<BufferAttr_HipcPointer> order,
                                       s32 offset) {
    EnsureBuiltinNewsLoaded();

    auto record_size = sizeof(NewsRecordV1);

    if (out_buffer.size() < record_size) {
        *out_count = 0;
        R_SUCCEED();
    }

    std::memset(out_buffer.data(), 0, out_buffer.size());

    const auto list = NewsStorage::Instance().ListAll();
    const size_t start = static_cast<size_t>(std::max(0, offset));
    const size_t max_records = out_buffer.size() / record_size;
    const size_t available = start < list.size() ? list.size() - start : 0;
    const size_t count = std::min(max_records, available);

    for (size_t i = 0; i < count; ++i) {
        const auto& src = list[start + i];
        NewsRecordV1 dst{};
        std::memcpy(dst.news_id.data(), src.news_id.data(), dst.news_id.size());
        std::memcpy(dst.user_id.data(), src.user_id.data(), dst.user_id.size());
        dst.received_time = src.received_time;
        dst.read = src.read;
        dst.newly = src.newly;
        dst.displayed = src.displayed;
        dst.extra1 = src.extra1;
        std::memcpy(out_buffer.data() + i * record_size, &dst, record_size);
    }

    *out_count = static_cast<s32>(count);
    R_SUCCEED();
}

Result INewsDatabaseService::GetList(Out<s32> out_count,
                                     OutBuffer<BufferAttr_HipcMapAlias> out_buffer,
                                     InBuffer<BufferAttr_HipcPointer> where,
                                     InBuffer<BufferAttr_HipcPointer> order,
                                     s32 offset) {
    EnsureBuiltinNewsLoaded();
    NewsStorage::Instance().ResetOpenCounter();

    auto record_size = sizeof(NewsRecord);

    if (out_buffer.size() < record_size) {
        *out_count = 0;
        R_SUCCEED();
    }

    std::memset(out_buffer.data(), 0, out_buffer.size());

    const auto list = NewsStorage::Instance().ListAll();
    const size_t start = static_cast<size_t>(std::max(0, offset));
    const size_t max_records = out_buffer.size() / record_size;
    const size_t available = start < list.size() ? list.size() - start : 0;
    const size_t count = std::min(max_records, available);

    if (count > 0) {
        std::memcpy(out_buffer.data(), list.data() + start, count * record_size);
    }

    *out_count = static_cast<s32>(count);
    R_SUCCEED();
}

} // namespace Service::News
