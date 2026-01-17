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

std::mutex mutex;
CacheMap cache;
bool loaded = false;

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
    if (loaded) {
        return;
    }

    loaded = true;

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
            if (value.is_number_integer()) {
                cache[key] = value.get<s64>();
            }
        }
    } catch (const std::exception& e) {
        LOG_WARNING(Core, "Failed to parse launch timestamp cache");
    }
}

void Save() {
    nlohmann::json json = nlohmann::json::object();
    for (const auto& [key, value] : cache) {
        json[fmt::format("{:016X}", key)] = value;
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
    std::scoped_lock lk{mutex};
    Load();
    cache[title_id] = NowSeconds();
    Save();
}

s64 GetLaunchTimestamp(u64 title_id) {
    std::scoped_lock lk{mutex};
    Load();
    const auto it = cache.find(title_id);
    if (it != cache.end()) {
        return it->second;
    }
    // we need a timestamp, i decided on 01/01/2026 00:00
    return 1767225600;
}

} // namespace Core::LaunchTimestampCache
