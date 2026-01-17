// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/bcat/news/news_storage.h"

#include "common/fs/path_util.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>

namespace Service::News {
namespace {

std::filesystem::path GetReadCachePath() {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "news" / "news_read";
}

std::set<std::string> LoadReadIds() {
    std::set<std::string> ids;
    std::ifstream f(GetReadCachePath());
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) ids.insert(line);
    }
    return ids;
}

void SaveReadIds(const std::set<std::string>& ids) {
    const auto path = GetReadCachePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream f(path);
    for (const auto& id : ids) {
        f << id << '\n';
    }
}

} // namespace

NewsStorage& NewsStorage::Instance() {
    static NewsStorage s;
    return s;
}

void NewsStorage::Clear() {
    std::scoped_lock lk{mtx};
    items.clear();
}

void NewsStorage::CopyZ(std::span<char> dst, std::string_view src) {
    std::memset(dst.data(), 0, dst.size());
    std::memcpy(dst.data(), src.data(), std::min(dst.size() - 1, src.size()));
}

std::string NewsStorage::MakeKey(std::string_view news_id, std::string_view user_id) {
    return std::string(news_id) + "|" + std::string(user_id);
}

s64 NewsStorage::Now() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

StoredNews& NewsStorage::Upsert(std::string_view news_id, std::string_view user_id,
                                std::string_view topic_id, s64 time, std::vector<u8> payload) {
    std::scoped_lock lk{mtx};

    const auto key = MakeKey(news_id, user_id);
    auto it = items.find(key);
    const bool exists = it != items.end();

    // implemented a little "read" file. All in cache.
    const auto read_ids = LoadReadIds();
    const bool was_read = read_ids.contains(std::string(news_id));

    NewsRecord rec{};
    CopyZ(rec.news_id, news_id);
    CopyZ(rec.user_id, user_id);
    // there is nx_notice and nx_promotion for tags pre-existing
    CopyZ(rec.topic_id, topic_id.empty() ? "nx_notice" : topic_id);

    rec.received_time = exists ? it->second.record.received_time : (time ? time : Now());
    rec.read = was_read ? 1 : (exists ? it->second.record.read : 0);
    rec.newly = was_read ? 0 : 1;
    rec.displayed = exists ? it->second.record.displayed : 0;
    rec.extra1 = exists ? it->second.record.extra1 : 0;
    rec.extra2 = exists ? it->second.record.extra2 : 0;

    auto& entry = items[key];
    entry.record = rec;
    entry.payload = std::move(payload);
    return entry;
}

StoredNews& NewsStorage::UpsertRaw(const GithubNewsMeta& meta, std::vector<u8> payload) {
    return Upsert(meta.news_id, "", meta.topic_id, static_cast<s64>(meta.published_at), std::move(payload));
}

std::vector<NewsRecord> NewsStorage::ListAll() const {
    std::scoped_lock lk{mtx};

    std::vector<NewsRecord> out;
    out.reserve(items.size());
    for (const auto& [_, v] : items) {
        out.push_back(v.record);
    }

    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return a.received_time > b.received_time;
    });
    return out;
}

std::optional<StoredNews> NewsStorage::FindByNewsId(std::string_view news_id,
                                                    std::string_view user_id) const {
    std::scoped_lock lk{mtx};

    if (auto it = items.find(MakeKey(news_id, user_id)); it != items.end()) {
        return it->second;
    }
    if (!user_id.empty()) {
        if (auto it = items.find(MakeKey(news_id, "")); it != items.end()) {
            return it->second;
        }
    }
    return std::nullopt;
}

bool NewsStorage::UpdateRecord(std::string_view news_id, std::string_view user_id,
                               const std::function<void(NewsRecord&)>& updater) {
    std::scoped_lock lk{mtx};

    if (auto it = items.find(MakeKey(news_id, user_id)); it != items.end()) {
        updater(it->second.record);
        return true;
    }
    return false;
}

void NewsStorage::MarkAsRead(std::string_view news_id) {
    std::scoped_lock lk{mtx};
    for (auto& [_, entry] : items) {
        if (std::string_view(entry.record.news_id.data()) == news_id) {
            entry.record.read = 1;
            entry.record.newly = 0;
            break;
        }
    }
    auto ids = LoadReadIds();
    ids.insert(std::string(news_id));
    SaveReadIds(ids);
}

size_t NewsStorage::GetAndIncrementOpenCounter() {
    std::scoped_lock lk{mtx};
    return open_counter++;
}

void NewsStorage::ResetOpenCounter() {
    std::scoped_lock lk{mtx};
    open_counter = 0;
}


} // namespace Service::News
