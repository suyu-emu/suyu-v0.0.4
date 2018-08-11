// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/optional.hpp>
#include "core/hle/service/acc/profile_manager.h"
#include "core/settings.h"

namespace Service::Account {
// TODO(ogniK): Get actual error codes
constexpr ResultCode ERROR_TOO_MANY_USERS(ErrorModule::Account, -1);
constexpr ResultCode ERROR_USER_ALREADY_EXISTS(ErrorModule::Account, -2);
constexpr ResultCode ERROR_ARGUMENT_IS_NULL(ErrorModule::Account, 20);

ProfileManager::ProfileManager() {
    // TODO(ogniK): Create the default user we have for now until loading/saving users is added
    auto user_uuid = UUID{1, 0};
    CreateNewUser(user_uuid, Settings::values.username);
    OpenUser(user_uuid);
}

boost::optional<size_t> ProfileManager::AddToProfiles(const ProfileInfo& user) {
    if (user_count >= MAX_USERS) {
        return boost::none;
    }
    profiles[user_count] = std::move(user);
    return user_count++;
}

bool ProfileManager::RemoveProfileAtIndex(size_t index) {
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

ResultCode ProfileManager::AddUser(ProfileInfo user) {
    if (AddToProfiles(user) == boost::none) {
        return ERROR_TOO_MANY_USERS;
    }
    return RESULT_SUCCESS;
}

ResultCode ProfileManager::CreateNewUser(UUID uuid, std::array<u8, 0x20>& username) {
    if (user_count == MAX_USERS) {
        return ERROR_TOO_MANY_USERS;
    }
    if (!uuid) {
        return ERROR_ARGUMENT_IS_NULL;
    }
    if (username[0] == 0x0) {
        return ERROR_ARGUMENT_IS_NULL;
    }
    if (std::any_of(profiles.begin(), profiles.end(),
                    [&uuid](const ProfileInfo& profile) { return uuid == profile.user_uuid; })) {
        return ERROR_USER_ALREADY_EXISTS;
    }
    ProfileInfo profile;
    profile.user_uuid = std::move(uuid);
    profile.username = std::move(username);
    profile.data = {};
    profile.creation_time = 0x0;
    profile.is_open = false;
    return AddUser(profile);
}

ResultCode ProfileManager::CreateNewUser(UUID uuid, const std::string& username) {
    std::array<u8, 0x20> username_output;
    if (username.size() > username_output.size()) {
        std::copy_n(username.begin(), username_output.size(), username_output.begin());
    } else {
        std::copy(username.begin(), username.end(), username_output.begin());
    }
    return CreateNewUser(uuid, username_output);
}

boost::optional<size_t> ProfileManager::GetUserIndex(const UUID& uuid) const {
    if (!uuid) {
        return boost::none;
    }
    auto iter = std::find_if(profiles.begin(), profiles.end(),
                             [&uuid](const ProfileInfo& p) { return p.user_uuid == uuid; });
    if (iter == profiles.end()) {
        return boost::none;
    }
    return static_cast<size_t>(std::distance(profiles.begin(), iter));
}

boost::optional<size_t> ProfileManager::GetUserIndex(ProfileInfo user) const {
    return GetUserIndex(user.user_uuid);
}

bool ProfileManager::GetProfileBase(boost::optional<size_t> index, ProfileBase& profile) const {
    if (index == boost::none || index >= MAX_USERS) {
        return false;
    }
    const auto& prof_info = profiles[index.get()];
    profile.user_uuid = prof_info.user_uuid;
    profile.username = prof_info.username;
    profile.timestamp = prof_info.creation_time;
    return true;
}

bool ProfileManager::GetProfileBase(UUID uuid, ProfileBase& profile) const {
    auto idx = GetUserIndex(uuid);
    return GetProfileBase(idx, profile);
}

bool ProfileManager::GetProfileBase(ProfileInfo user, ProfileBase& profile) const {
    return GetProfileBase(user.user_uuid, profile);
}

size_t ProfileManager::GetUserCount() const {
    return user_count;
}

size_t ProfileManager::GetOpenUserCount() const {
    return std::count_if(profiles.begin(), profiles.end(),
                         [](const ProfileInfo& p) { return p.is_open; });
}

bool ProfileManager::UserExists(UUID uuid) const {
    return (GetUserIndex(uuid) != boost::none);
}

void ProfileManager::OpenUser(UUID uuid) {
    auto idx = GetUserIndex(uuid);
    if (idx == boost::none) {
        return;
    }
    profiles[idx.get()].is_open = true;
    last_opened_user = uuid;
}

void ProfileManager::CloseUser(UUID uuid) {
    auto idx = GetUserIndex(uuid);
    if (idx == boost::none) {
        return;
    }
    profiles[idx.get()].is_open = false;
}

std::array<UUID, MAX_USERS> ProfileManager::GetAllUsers() const {
    std::array<UUID, MAX_USERS> output;
    std::transform(profiles.begin(), profiles.end(), output.begin(),
                   [](const ProfileInfo& p) { return p.user_uuid; });
    return output;
}

std::array<UUID, MAX_USERS> ProfileManager::GetOpenUsers() const {
    std::array<UUID, MAX_USERS> output;
    std::transform(profiles.begin(), profiles.end(), output.begin(), [](const ProfileInfo& p) {
        if (p.is_open)
            return p.user_uuid;
        return UUID{};
    });
    std::stable_partition(output.begin(), output.end(), [](const UUID& uuid) { return uuid; });
    return output;
}

UUID ProfileManager::GetLastOpenedUser() const {
    return last_opened_user;
}

bool ProfileManager::GetProfileBaseAndData(boost::optional<size_t> index, ProfileBase& profile,
                                           std::array<u8, MAX_DATA>& data) const {
    if (GetProfileBase(index, profile)) {
        std::memcpy(data.data(), profiles[index.get()].data.data(), MAX_DATA);
        return true;
    }
    return false;
}

bool ProfileManager::GetProfileBaseAndData(UUID uuid, ProfileBase& profile,
                                           std::array<u8, MAX_DATA>& data) const {
    auto idx = GetUserIndex(uuid);
    return GetProfileBaseAndData(idx, profile, data);
}

bool ProfileManager::GetProfileBaseAndData(ProfileInfo user, ProfileBase& profile,
                                           std::array<u8, MAX_DATA>& data) const {
    return GetProfileBaseAndData(user.user_uuid, profile, data);
}

bool ProfileManager::CanSystemRegisterUser() const {
    return false; // TODO(ogniK): Games shouldn't have
                  // access to user registration, when we
    // emulate qlaunch. Update this to dynamically change.
}

}; // namespace Service::Account
