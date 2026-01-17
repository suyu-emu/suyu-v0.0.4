// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/common_types.h"

namespace Service::News {

struct GithubNewsMeta {
    std::string news_id;
    std::string topic_id;
    u64 published_at{};
    u64 pickup_limit{};
    u64 essential_pickup_limit{};
    u64 expire_at{};
    u32 priority{};
    u32 deletion_priority{100};
    u32 decoration_type{1};
    u32 opted_in{1};
    u32 essential_pickup_limit_flag{1};
    u32 category{};
    u32 language_mask{1};
};

struct NewsRecordV1 {
    std::array<char, 24> news_id{};
    std::array<char, 24> user_id{};
    s64 received_time{};
    s32 read{};
    s32 newly{};
    s32 displayed{};
    s32 extra1{};
};
static_assert(sizeof(NewsRecordV1) == 72);

struct NewsRecord {
    std::array<char, 24> news_id{};
    std::array<char, 24> user_id{};
    std::array<char, 32> topic_id{};
    s64 received_time{};
    std::array<u8, 12> _pad1{};
    s32 read{};
    s32 newly{};
    s32 displayed{};
    std::array<u8, 8> _pad2{};
    s32 extra1{};
    s32 extra2{};
};
static_assert(sizeof(NewsRecord) == 128);

struct StoredNews {
    NewsRecord record{};
    std::vector<u8> payload;
};

class NewsStorage {
public:
    static NewsStorage& Instance();

    void Clear();
    StoredNews& Upsert(std::string_view news_id, std::string_view user_id,
                       std::string_view topic_id, s64 time, std::vector<u8> payload);
    StoredNews& UpsertRaw(const GithubNewsMeta& meta, std::vector<u8> payload);

    std::vector<NewsRecord> ListAll() const;
    std::optional<StoredNews> FindByNewsId(std::string_view news_id,
                                           std::string_view user_id = {}) const;
    bool UpdateRecord(std::string_view news_id, std::string_view user_id,
                      const std::function<void(NewsRecord&)>& updater);
    void MarkAsRead(std::string_view news_id);

    size_t GetAndIncrementOpenCounter();
    void ResetOpenCounter();

private:
    NewsStorage() = default;

    static std::string MakeKey(std::string_view news_id, std::string_view user_id);
    static void CopyZ(std::span<char> dst, std::string_view src);
    static s64 Now();

    mutable std::mutex mtx;
    std::unordered_map<std::string, StoredNews> items;
    size_t open_counter{};
};

} // namespace Service::News
