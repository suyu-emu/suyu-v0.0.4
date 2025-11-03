// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <random>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/find.hpp>
#include <fmt/ranges.h>

#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/fs_types.h"
#include "common/fs/path_util.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/file_sys/savedata_factory.h"
#include "core/hle/service/acc/profile_manager.h"
#include <ranges>

namespace Service::Account {

namespace FS = Common::FS;
using Common::UUID;

struct UserRaw {
    UUID uuid{};
    UUID uuid2{};
    u64 timestamp{};
    ProfileUsername username{};
    UserData extra_data{};
};
static_assert(sizeof(UserRaw) == 0xC8, "UserRaw has incorrect size.");

struct ProfileDataRaw {
    INSERT_PADDING_BYTES(0x10);
    std::array<UserRaw, MAX_USERS> users{};
};

static_assert(sizeof(ProfileDataRaw) == 0x650, "ProfileDataRaw has incorrect size.");

// TODO(ogniK): Get actual error codes
constexpr Result ERROR_TOO_MANY_USERS(ErrorModule::Account, u32(-1));
constexpr Result ERROR_USER_ALREADY_EXISTS(ErrorModule::Account, u32(-2));
constexpr Result ERROR_ARGUMENT_IS_NULL(ErrorModule::Account, 20);

constexpr char ACC_SAVE_AVATORS_BASE_PATH[] = "system/save/8000000000000010/su/avators";

ProfileManager::ProfileManager() {
    ParseUserSaveFile();

    // Create an user if none are present
    if (user_count == 0) {
        CreateNewUser(UUID::MakeRandom(), "Eden");
        WriteUserSaveFile();
    }

    auto current =
        std::clamp<int>(static_cast<s32>(Settings::values.current_user), 0, MAX_USERS - 1);

    // If user index don't exist. Load the first user and change the active user
    if (!UserExistsIndex(current)) {
        current = 0;
        Settings::values.current_user = 0;
    }

    OpenUser(*GetUser(current));
}

ProfileManager::~ProfileManager() = default;

/// After a users creation it needs to be "registered" to the system. AddToProfiles handles the
/// internal management of the users profiles
std::optional<std::size_t> ProfileManager::AddToProfiles(const ProfileInfo& profile) {
    if (user_count >= MAX_USERS) {
        return std::nullopt;
    }
    profiles[user_count] = profile;
    return user_count++;
}

/// Deletes a specific profile based on it's profile index
bool ProfileManager::RemoveProfileAtIndex(std::size_t index) {
    if (index >= MAX_USERS || index >= user_count) {
        return false;
    }
    if (index < user_count - 1) {
        std::rotate(profiles.begin() + index, profiles.begin() + index + 1, profiles.end());
    }
    profiles.back() = {};
    user_count--;
    return true;
}

void ProfileManager::RemoveAllProfiles()
{
    profiles = {};
}

/// Helper function to register a user to the system
Result ProfileManager::AddUser(const ProfileInfo& user) {
    if (!AddToProfiles(user)) {
        return ERROR_TOO_MANY_USERS;
    }
    return ResultSuccess;
}

/// Create a new user on the system. If the uuid of the user already exists, the user is not
/// created.
Result ProfileManager::CreateNewUser(UUID uuid, const ProfileUsername& username) {
    if (user_count == MAX_USERS) {
        return ERROR_TOO_MANY_USERS;
    }
    if (uuid.IsInvalid()) {
        return ERROR_ARGUMENT_IS_NULL;
    }
    if (username[0] == 0x0) {
        return ERROR_ARGUMENT_IS_NULL;
    }
    if (std::any_of(profiles.begin(), profiles.end(),
                    [&uuid](const ProfileInfo& profile) { return uuid == profile.user_uuid; })) {
        return ERROR_USER_ALREADY_EXISTS;
    }

    is_save_needed = true;

    return AddUser({
        .user_uuid = uuid,
        .username = username,
        .creation_time = 0,
        .data = {},
        .is_open = false,
    });
}

/// Creates a new user on the system. This function allows a much simpler method of registration
/// specifically by allowing an std::string for the username. This is required specifically since
/// we're loading a string straight from the config
Result ProfileManager::CreateNewUser(UUID uuid, const std::string& username) {
    ProfileUsername username_output{};

    if (username.size() > username_output.size()) {
        std::copy_n(username.begin(), username_output.size(), username_output.begin());
    } else {
        std::copy(username.begin(), username.end(), username_output.begin());
    }
    return CreateNewUser(uuid, username_output);
}

std::optional<UUID> ProfileManager::GetUser(std::size_t index) const {
    if (index >= MAX_USERS) {
        return std::nullopt;
    }

    return profiles[index].user_uuid;
}

/// Returns a users profile index based on their user id.
std::optional<std::size_t> ProfileManager::GetUserIndex(const UUID& uuid) const {
    if (uuid.IsInvalid()) {
        return std::nullopt;
    }

    const auto iter = std::find_if(profiles.begin(), profiles.end(),
                                   [&uuid](const ProfileInfo& p) { return p.user_uuid == uuid; });
    if (iter == profiles.end()) {
        return std::nullopt;
    }

    return static_cast<std::size_t>(std::distance(profiles.begin(), iter));
}

/// Returns a users profile index based on their profile
std::optional<std::size_t> ProfileManager::GetUserIndex(const ProfileInfo& user) const {
    return GetUserIndex(user.user_uuid);
}

/// Returns the first user profile seen based on username (which does not enforce uniqueness)
std::optional<std::size_t> ProfileManager::GetUserIndex(const std::string& username) const {
    const auto iter =
        std::find_if(profiles.begin(), profiles.end(), [&username](const ProfileInfo& p) {
            const std::string profile_username = Common::StringFromFixedZeroTerminatedBuffer(
                reinterpret_cast<const char*>(p.username.data()), p.username.size());

            return username.compare(profile_username) == 0;
        });
    if (iter == profiles.end()) {
        return std::nullopt;
    }

    return static_cast<std::size_t>(std::distance(profiles.begin(), iter));
}

/// Returns the data structure used by the switch when GetProfileBase is called on acc:*
bool ProfileManager::GetProfileBase(std::optional<std::size_t> index, ProfileBase& profile) const {
    if (!index || index >= MAX_USERS) {
        return false;
    }
    const auto& prof_info = profiles[*index];
    profile.user_uuid = prof_info.user_uuid;
    profile.username = prof_info.username;
    profile.timestamp = prof_info.creation_time;
    return true;
}

/// Returns the data structure used by the switch when GetProfileBase is called on acc:*
bool ProfileManager::GetProfileBase(UUID uuid, ProfileBase& profile) const {
    const auto idx = GetUserIndex(uuid);
    return GetProfileBase(idx, profile);
}

/// Returns the data structure used by the switch when GetProfileBase is called on acc:*
bool ProfileManager::GetProfileBase(const ProfileInfo& user, ProfileBase& profile) const {
    return GetProfileBase(user.user_uuid, profile);
}

/// Returns the current user count on the system. We keep a variable which tracks the count so we
/// don't have to loop the internal profile array every call.

std::size_t ProfileManager::GetUserCount() const {
    return user_count;
}

/// Lists the current "opened" users on the system. Users are typically not open until they sign
/// into something or pick a profile. As of right now users should all be open until qlaunch is
/// booting

std::size_t ProfileManager::GetOpenUserCount() const {
    return std::count_if(profiles.begin(), profiles.end(),
                         [](const ProfileInfo& p) { return p.is_open; });
}

/// Checks if a user id exists in our profile manager
bool ProfileManager::UserExists(UUID uuid) const {
    return GetUserIndex(uuid).has_value();
}

bool ProfileManager::UserExistsIndex(std::size_t index) const {
    if (index >= MAX_USERS) {
        return false;
    }
    return profiles[index].user_uuid.IsValid();
}

/// Opens a specific user
void ProfileManager::OpenUser(UUID uuid) {
    const auto idx = GetUserIndex(uuid);
    if (!idx) {
        return;
    }

    profiles[*idx].is_open = true;
    last_opened_user = uuid;
}

/// Closes a specific user
void ProfileManager::CloseUser(UUID uuid) {
    const auto idx = GetUserIndex(uuid);
    if (!idx) {
        return;
    }

    profiles[*idx].is_open = false;
}

/// Gets all valid user ids on the system
UserIDArray ProfileManager::GetAllUsers() const {
    UserIDArray output{};
    std::ranges::transform(profiles, output.begin(), [](const ProfileInfo& p) {
        return p.user_uuid;
    });
    return output;
}

/// Get all the open users on the system and zero out the rest of the data. This is specifically
/// needed for GetOpenUsers and we need to ensure the rest of the output buffer is zero'd out
UserIDArray ProfileManager::GetOpenUsers() const {
    UserIDArray output{};
    std::ranges::transform(profiles, output.begin(), [](const ProfileInfo& p) {
        if (p.is_open)
            return p.user_uuid;
        return Common::InvalidUUID;
    });
    std::stable_partition(output.begin(), output.end(),
                          [](const UUID& uuid) { return uuid.IsValid(); });
    return output;
}

/// Returns the last user which was opened
UUID ProfileManager::GetLastOpenedUser() const {
    return last_opened_user;
}

/// Gets the list of stored opened users.
UserIDArray ProfileManager::GetStoredOpenedUsers() const {
    UserIDArray output{};
    std::ranges::transform(stored_opened_profiles, output.begin(), [](const ProfileInfo& p) {
        if (p.is_open)
            return p.user_uuid;
        return Common::InvalidUUID;
    });
    std::stable_partition(output.begin(), output.end(),
                          [](const UUID& uuid) { return uuid.IsValid(); });
    return output;
}

/// Captures the opened users, which can be queried across process launches with
/// ListOpenContextStoredUsers.
void ProfileManager::StoreOpenedUsers() {
    size_t profile_index{};
    stored_opened_profiles = {};
    std::for_each(profiles.begin(), profiles.end(), [&](const auto& profile) {
        if (profile.is_open) {
            stored_opened_profiles[profile_index++] = profile;
        }
    });
}

/// Return the users profile base and the unknown arbitrary data.
bool ProfileManager::GetProfileBaseAndData(std::optional<std::size_t> index, ProfileBase& profile,
                                           UserData& data) const {
    if (GetProfileBase(index, profile)) {
        data = profiles[*index].data;
        return true;
    }
    return false;
}

/// Return the users profile base and the unknown arbitrary data.
bool ProfileManager::GetProfileBaseAndData(UUID uuid, ProfileBase& profile, UserData& data) const {
    const auto idx = GetUserIndex(uuid);
    return GetProfileBaseAndData(idx, profile, data);
}

/// Return the users profile base and the unknown arbitrary data.
bool ProfileManager::GetProfileBaseAndData(const ProfileInfo& user, ProfileBase& profile,
                                           UserData& data) const {
    return GetProfileBaseAndData(user.user_uuid, profile, data);
}

/// Returns if the system is allowing user registrations or not
bool ProfileManager::CanSystemRegisterUser() const {
    // TODO: Both games and applets can register users. Determine when this condition is not meet.
    return true;
}

bool ProfileManager::RemoveUser(UUID uuid) {
    const auto index = GetUserIndex(uuid);
    if (!index) {
        return false;
    }

    profiles[*index] = ProfileInfo{};
    std::stable_partition(profiles.begin(), profiles.end(),
                          [](const ProfileInfo& profile) { return profile.user_uuid.IsValid(); });

    is_save_needed = true;
    WriteUserSaveFile();

    return true;
}

bool ProfileManager::SetProfileBase(UUID uuid, const ProfileBase& profile_new) {
    const auto index = GetUserIndex(uuid);
    if (!index || profile_new.user_uuid.IsInvalid()) {
        return false;
    }

    auto& profile = profiles[*index];
    profile.user_uuid = profile_new.user_uuid;
    profile.username = profile_new.username;
    profile.creation_time = profile_new.timestamp;

    is_save_needed = true;
    WriteUserSaveFile();

    return true;
}

bool ProfileManager::SetProfileBaseAndData(Common::UUID uuid, const ProfileBase& profile_new,
                                           const UserData& data_new) {
    const auto index = GetUserIndex(uuid);
    if (index.has_value() && SetProfileBase(uuid, profile_new)) {
        profiles[*index].data = data_new;
        is_save_needed = true;
        WriteUserSaveFile();
        return true;
    } else {
        LOG_ERROR(Service_ACC, "Failed to set profile base and data for user with UUID: {}",
                  uuid.RawString());
    }
    return false;
}

void ProfileManager::ParseUserSaveFile() {
    const auto save_path(FS::GetEdenPath(FS::EdenPath::NANDDir) / ACC_SAVE_AVATORS_BASE_PATH /
                         "profiles.dat");

    const FS::IOFile save(save_path, FS::FileAccessMode::Read, FS::FileType::BinaryFile);

    if (!save.IsOpen()) {
        LOG_WARNING(Service_ACC, "Failed to load profile data from save data... Generating new "
                                 "user 'Eden' with random UUID.");
        return;
    }

    ProfileDataRaw data;
    if (!save.ReadObject(data)) {
        LOG_WARNING(Service_ACC, "profiles.dat is smaller than expected... Generating new user "
                                 "'Eden' with random UUID.");
        return;
    }

    for (const auto& user : data.users) {
        if (user.uuid.IsInvalid()) {
            continue;
        }

        AddUser({
            .user_uuid = user.uuid,
            .username = user.username,
            .creation_time = user.timestamp,
            .data = user.extra_data,
            .is_open = false,
        });
    }

    std::stable_partition(profiles.begin(), profiles.end(),
                          [](const ProfileInfo& profile) { return profile.user_uuid.IsValid(); });
}

void ProfileManager::WriteUserSaveFile() {
    if (!is_save_needed) {
        return;
    }

    ProfileDataRaw raw{};

    for (std::size_t i = 0; i < MAX_USERS; ++i) {
        raw.users[i] = {
            .uuid = profiles[i].user_uuid,
            .uuid2 = profiles[i].user_uuid,
            .timestamp = profiles[i].creation_time,
            .username = profiles[i].username,
            .extra_data = profiles[i].data,
        };
    }

    const auto raw_path(FS::GetEdenPath(FS::EdenPath::NANDDir) / "system/save/8000000000000010");
    if (FS::IsFile(raw_path) && !FS::RemoveFile(raw_path)) {
        return;
    }

    const auto save_path(FS::GetEdenPath(FS::EdenPath::NANDDir) / ACC_SAVE_AVATORS_BASE_PATH /
                         "profiles.dat");

    if (FS::IsFile(save_path) && !FS::RemoveFile(save_path)) {
        LOG_WARNING(Service_ACC, "Could not remove existing profiles.dat");
        return;
    } else {
        LOG_INFO(Service_ACC, "Writing profiles.dat to {}",
                 Common::FS::PathToUTF8String(save_path));
    }

    if (!FS::CreateParentDirs(save_path)) {
        LOG_WARNING(Service_ACC, "Failed to create full path of profiles.dat. Create the directory "
                                 "nand/system/save/8000000000000010/su/avators to mitigate this "
                                 "issue.");
        return;
    }

    FS::IOFile save(save_path, FS::FileAccessMode::Write, FS::FileType::BinaryFile);

    if (!save.IsOpen() || !save.SetSize(sizeof(ProfileDataRaw)) || !save.WriteObject(raw)) {
        LOG_WARNING(Service_ACC, "Failed to write save data to file... No changes to user data "
                                 "made in current session will be saved.");
        return;
    }

    is_save_needed = false;
}

void ProfileManager::ResetUserSaveFile()
{
    RemoveAllProfiles();
    ParseUserSaveFile();
}

std::vector<std::string> ProfileManager::FindGoodProfiles()
{
    namespace fs = std::filesystem;

    std::vector<std::string> good_uuids;

    const auto path = Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir)
                      / "user/save/0000000000000000";

    // some exceptions because certain games just LOVE TO CAUSE ISSUES
    static constexpr const std::array<const char* const, 2> EXCEPTION_UUIDS
        = {"5755CC2A545A87128500000000000000", "00000000000000000000000000000000"};

    for (const char *const uuid : EXCEPTION_UUIDS) {
        if (fs::exists(path / uuid))
            good_uuids.emplace_back(uuid);
    }

    for (const ProfileInfo& p : profiles) {
        std::string uuid_string = [p]() -> std::string {
            auto uuid = p.user_uuid;

            // "ignore" invalid uuids
            if (uuid.IsInvalid()) {
                return "0";
            }

            auto user_id = uuid.AsU128();

            return fmt::format("{:016X}{:016X}", user_id[1], user_id[0]);
        }();

        if (uuid_string != "0") good_uuids.emplace_back(uuid_string);
    }

    return good_uuids;
}

std::vector<std::string> ProfileManager::FindOrphanedProfiles()
{
    std::vector<std::string> good_uuids = FindGoodProfiles();

    namespace fs = std::filesystem;

    // TODO: fetch save_id programmatically
    const auto path = Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir)
                      / "user/save/0000000000000000";

    std::vector<std::string> orphaned_profiles;

    Common::FS::IterateDirEntries(
        path,
        [&good_uuids, &orphaned_profiles](const std::filesystem::directory_entry& entry) -> bool {
            const std::string uuid = entry.path().stem().string();

            bool override = false;

            // first off, we should always clear empty profiles
            // 99% of the time these are useless. If not, they are recreated anyways...
            const auto is_empty = [&entry, &override]() -> bool {
                try {
                    for (const auto& file : fs::recursive_directory_iterator(entry.path())) {
                        // TODO: .yuzu_save_size is a weird file that gets created by certain games
                        // I have no idea what its purpose is, but TEMPORARY SOLUTION: just mark the profile as valid if
                        // this file exists (???) e.g. for SSBU
                        // In short: if .yuzu_save_size is the ONLY file in a profile it's probably fine to keep
                        if (file.path().filename().string() == FileSys::GetSaveDataSizeFileName())
                            override = true;

                        // if there are any regular files (NOT directories) there, do NOT delete it :p
                        if (file.is_regular_file())
                            return false;
                    }
                } catch (const fs::filesystem_error& e) {
                    // if we get an error--no worries, just pretend it's not empty
                    return true;
                }

                return true;
            }();

            if (is_empty) {
                fs::remove_all(entry);
                return true;
            }

            // edge-case: some filesystems forcefully change filenames to lowercase
            // so we can just ignore any differences
            // looking at you microsoft... ;)
            std::string upper_uuid = uuid;
            boost::to_upper(upper_uuid);

            // if profiles.dat contains the UUID--all good
            // if not--it's an orphaned profile and should be resolved by the user
            if (!override
                && std::find(good_uuids.begin(), good_uuids.end(), upper_uuid) == good_uuids.end()) {
                orphaned_profiles.emplace_back(uuid);
            }
            return true;
        },
        Common::FS::DirEntryFilter::Directory);

    return orphaned_profiles;
}

void ProfileManager::SetUserPosition(u64 position, Common::UUID uuid) {
    auto idxOpt = GetUserIndex(uuid);
    if (!idxOpt)
        return;

    size_t oldIdx = *idxOpt;
    if (position >= user_count || position == oldIdx)
        return;

    ProfileInfo moving = profiles[oldIdx];

    if (position < oldIdx) {
        for (size_t i = oldIdx; i > position; --i)
            profiles[i] = profiles[i - 1];
    } else {
        for (size_t i = oldIdx; i < position; ++i)
            profiles[i] = profiles[i + 1];
    }

    profiles[position] = std::move(moving);

    is_save_needed = true;
    WriteUserSaveFile();
}


}; // namespace Service::Account
