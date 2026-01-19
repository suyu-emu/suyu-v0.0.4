// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/launch_timestamp_cache.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "common/fs/fs.h"
#include "common/fs/file.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"

namespace Core::LaunchTimestampCache {
namespace {

using CacheMap = std::unordered_map<u64, s64>;
using CountMap = std::unordered_map<u64, u64>;

std::mutex g_mutex;
CacheMap g_cache;
CountMap g_counts;
bool g_loaded = false;

std::filesystem::path GetCachePath() {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "launched.json";
}

std::optional<std::string> ReadFileToString(const std::filesystem::path& path) {
    const std::ifstream file{path, std::ios::in | std::ios::binary};
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool WriteStringToFile(const std::filesystem::path& path, const std::string& data) {
    if (!Common::FS::CreateParentDirs(path)) {
        return false;
    }
    std::ofstream file{path, std::ios::out | std::ios::binary | std::ios::trunc};
    if (!file) {
        return false;
    }
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(file);
}

void Load() {
    if (g_loaded) {
        return;
    }

    g_loaded = true;

    const auto path = GetCachePath();
    if (!std::filesystem::exists(path)) {
        return;
    }

    const auto data = ReadFileToString(path);
    if (!data) {
        LOG_WARNING(Core, "Failed to read launch timestamp cache: {}",
                    Common::FS::PathToUTF8String(path));
        return;
    }

    try {
        const auto json = nlohmann::json::parse(data->data(), data->data() + data->size());
        if (!json.is_object()) {
            return;
        }
        for (auto it = json.begin(); it != json.end(); ++it) {
            const auto key_str = it.key();
            const auto value = it.value();
            u64 key{};
            try {
                key = std::stoull(key_str, nullptr, 16);
            } catch (...) {
                continue;
            }
            if (value.is_object()) {
                if (value.contains("timestamp") && value["timestamp"].is_number_integer()) {
                    g_cache[key] = value["timestamp"].get<s64>();
                }
                if (value.contains("launch_count") && value["launch_count"].is_number_unsigned()) {
                    g_counts[key] = value["launch_count"].get<u64>();
                }
            } else if (value.is_number_integer()) {
                // Legacy format: raw timestamp only
                g_cache[key] = value.get<s64>();
            }
        }
    } catch (const std::exception& e) {
        LOG_WARNING(Core, "Failed to parse launch timestamp cache: {}", e.what());
    }
}

void Save() {
    nlohmann::json json = nlohmann::json::object();
    for (const auto& [key, value] : g_cache) {
        nlohmann::json entry = nlohmann::json::object();
        entry["timestamp"] = value;
        const auto count_it = g_counts.find(key);
        entry["launch_count"] = count_it != g_counts.end() ? count_it->second : 0;
        json[fmt::format("{:016X}", key)] = entry;
    }

    const auto path = GetCachePath();
    if (!WriteStringToFile(path, json.dump(4))) {
        LOG_WARNING(Core, "Failed to write launch timestamp cache: {}",
                    Common::FS::PathToUTF8String(path));
    }
}

s64 NowSeconds() {
    return std::time(nullptr);
}

} // namespace

void SaveLaunchTimestamp(u64 title_id) {
    std::scoped_lock lk{g_mutex};
    Load();
    g_cache[title_id] = NowSeconds();
    g_counts[title_id] = g_counts[title_id] + 1;
    Save();
}

s64 GetLaunchTimestamp(u64 title_id) {
    std::scoped_lock lk{g_mutex};
    Load();
    const auto it = g_cache.find(title_id);
    if (it != g_cache.end()) {
        return it->second;
    }
    // we need a timestamp, i decided on 01/01/2026 00:00
    return 1767225600;
}

u64 GetLaunchCount(u64 title_id) {
    std::scoped_lock lk{g_mutex};
    Load();
    const auto it = g_counts.find(title_id);
    if (it != g_counts.end()) {
        return it->second;
    }
    return 0;
}

} // namespace Core::LaunchTimestampCache
