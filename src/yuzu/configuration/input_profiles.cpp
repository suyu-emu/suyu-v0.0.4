// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fmt/format.h>

#include "common/common_paths.h"
#include "common/file_util.h"
#include "yuzu/configuration/config.h"
#include "yuzu/configuration/input_profiles.h"

namespace FS = Common::FS;

namespace {

bool ProfileExistsInFilesystem(std::string_view profile_name) {
    return FS::Exists(fmt::format("{}input" DIR_SEP "{}.ini",
                                  FS::GetUserPath(FS::UserPath::ConfigDir), profile_name));
}

bool IsINI(std::string_view filename) {
    const std::size_t index = filename.rfind('.');

    if (index == std::string::npos) {
        return false;
    }

    return filename.substr(index) == ".ini";
}

std::string GetNameWithoutExtension(const std::string& filename) {
    const std::size_t index = filename.rfind('.');

    if (index == std::string::npos) {
        return filename;
    }

    return filename.substr(0, index);
}

} // namespace

InputProfiles::InputProfiles() {
    const std::string input_profile_loc =
        fmt::format("{}input", FS::GetUserPath(FS::UserPath::ConfigDir));

    FS::ForeachDirectoryEntry(
        nullptr, input_profile_loc,
        [this](u64* entries_out, const std::string& directory, const std::string& filename) {
            if (IsINI(filename) && IsProfileNameValid(GetNameWithoutExtension(filename))) {
                map_profiles.insert_or_assign(
                    GetNameWithoutExtension(filename),
                    std::make_unique<Config>(GetNameWithoutExtension(filename),
                                             Config::ConfigType::InputProfile));
            }
            return true;
        });
}

InputProfiles::~InputProfiles() = default;

std::vector<std::string> InputProfiles::GetInputProfileNames() {
    std::vector<std::string> profile_names;
    profile_names.reserve(map_profiles.size());

    for (const auto& [profile_name, config] : map_profiles) {
        if (!ProfileExistsInFilesystem(profile_name)) {
            DeleteProfile(profile_name);
            continue;
        }

        profile_names.push_back(profile_name);
    }

    return profile_names;
}

bool InputProfiles::IsProfileNameValid(std::string_view profile_name) {
    return profile_name.find_first_of("<>:;\"/\\|,.!?*") == std::string::npos;
}

bool InputProfiles::CreateProfile(const std::string& profile_name, std::size_t player_index) {
    if (ProfileExistsInMap(profile_name)) {
        return false;
    }

    map_profiles.insert_or_assign(
        profile_name, std::make_unique<Config>(profile_name, Config::ConfigType::InputProfile));

    return SaveProfile(profile_name, player_index);
}

bool InputProfiles::DeleteProfile(const std::string& profile_name) {
    if (!ProfileExistsInMap(profile_name)) {
        return false;
    }

    if (!ProfileExistsInFilesystem(profile_name) ||
        FS::Delete(map_profiles[profile_name]->GetConfigFilePath())) {
        map_profiles.erase(profile_name);
    }

    return !ProfileExistsInMap(profile_name) && !ProfileExistsInFilesystem(profile_name);
}

bool InputProfiles::LoadProfile(const std::string& profile_name, std::size_t player_index) {
    if (!ProfileExistsInMap(profile_name)) {
        return false;
    }

    if (!ProfileExistsInFilesystem(profile_name)) {
        map_profiles.erase(profile_name);
        return false;
    }

    map_profiles[profile_name]->ReadControlPlayerValue(player_index);
    return true;
}

bool InputProfiles::SaveProfile(const std::string& profile_name, std::size_t player_index) {
    if (!ProfileExistsInMap(profile_name)) {
        return false;
    }

    map_profiles[profile_name]->SaveControlPlayerValue(player_index);
    return true;
}

bool InputProfiles::ProfileExistsInMap(const std::string& profile_name) const {
    return map_profiles.find(profile_name) != map_profiles.end();
}
