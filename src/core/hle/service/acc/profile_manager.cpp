#include "profile_manager.h"

namespace Service::Account {
// TODO(ogniK): Get actual error codes
constexpr ResultCode ERROR_TOO_MANY_USERS(ErrorModule::Account, -1);
constexpr ResultCode ERROR_ARGUMENT_IS_NULL(ErrorModule::Account, 20);

size_t ProfileManager::AddToProfiles(const ProfileInfo& user) {
    if (user_count >= MAX_USERS) {
        return -1;
    }
    profiles[user_count] = std::move(user);
    return user_count++;
}

bool ProfileManager::RemoveProfileAtIdx(size_t index) {
    if (index >= MAX_USERS || index < 0 || index >= user_count)
        return false;
    profiles[index] = ProfileInfo{};
    if (index < user_count - 1)
        for (size_t i = index; i < user_count - 1; i++)
            profiles[i] = profiles[i + 1]; // Shift upper profiles down
    user_count--;
    return true;
}

ResultCode ProfileManager::AddUser(ProfileInfo user) {
    if (AddToProfiles(user) == -1) {
        return ERROR_TOO_MANY_USERS;
    }
    return RESULT_SUCCESS;
}

ResultCode ProfileManager::CreateNewUser(UUID uuid, std::array<u8, 0x20> username) {
    if (user_count == MAX_USERS)
        return ERROR_TOO_MANY_USERS;
    if (!uuid)
        return ERROR_ARGUMENT_IS_NULL;
    if (username[0] == 0x0)
        return ERROR_ARGUMENT_IS_NULL;
    ProfileInfo prof_inf;
    prof_inf.user_uuid = uuid;
    prof_inf.username = username;
    prof_inf.data = std::array<u8, MAX_DATA>();
    prof_inf.creation_time = 0x0;
    prof_inf.is_open = false;
    return AddUser(prof_inf);
}

size_t ProfileManager::GetUserIndex(UUID uuid) {
    if (!uuid)
        return -1;
    for (unsigned i = 0; i < user_count; i++)
        if (profiles[i].user_uuid == uuid)
            return i;
    return -1;
}

size_t ProfileManager::GetUserIndex(ProfileInfo user) {
    return GetUserIndex(user.user_uuid);
}

bool ProfileManager::GetProfileBase(size_t index, ProfileBase& profile) {
    if (index >= MAX_USERS) {
        profile.Invalidate();
        return false;
    }
    auto prof_info = profiles[index];
    profile.user_uuid = prof_info.user_uuid;
    profile.username = prof_info.username;
    profile.timestamp = prof_info.creation_time;
    return true;
}

bool ProfileManager::GetProfileBase(UUID uuid, ProfileBase& profile) {
    auto idx = GetUserIndex(uuid);
    return GetProfileBase(idx, profile);
}

bool ProfileManager::GetProfileBase(ProfileInfo user, ProfileBase& profile) {
    return GetProfileBase(user.user_uuid, profile);
}

size_t ProfileManager::GetUserCount() {
    return user_count;
}

bool ProfileManager::UserExists(UUID uuid) {
    return (GetUserIndex(uuid) != -1);
}

void ProfileManager::OpenUser(UUID uuid) {
    auto idx = GetUserIndex(uuid);
    if (idx == -1)
        return;
    profiles[idx].is_open = true;
    last_openned_user = uuid;
}

void ProfileManager::CloseUser(UUID uuid) {
    auto idx = GetUserIndex(uuid);
    if (idx == -1)
        return;
    profiles[idx].is_open = false;
}

std::array<UUID, MAX_USERS> ProfileManager::GetAllUsers() {
    std::array<UUID, MAX_USERS> output;
    for (unsigned i = 0; i < user_count; i++) {
        output[i] = profiles[i].user_uuid;
    }
    return output;
}

std::array<UUID, MAX_USERS> ProfileManager::GetOpenUsers() {
    std::array<UUID, MAX_USERS> output;
    unsigned user_idx = 0;
    for (unsigned i = 0; i < user_count; i++) {
        if (profiles[i].is_open) {
            output[i++] = profiles[i].user_uuid;
        }
    }
    return output;
}

const UUID& ProfileManager::GetLastOpennedUser() {
    return last_openned_user;
}

bool ProfileManager::GetProfileBaseAndData(size_t index, ProfileBase& profile,
                                           std::array<u8, MAX_DATA>& data) {
    if (GetProfileBase(index, profile)) {
        std::memcpy(data.data(), profiles[index].data.data(), MAX_DATA);
        return true;
    }
    return false;
}
bool ProfileManager::GetProfileBaseAndData(UUID uuid, ProfileBase& profile,
                                           std::array<u8, MAX_DATA>& data) {
    auto idx = GetUserIndex(uuid);
    return GetProfileBaseAndData(idx, profile, data);
}

bool ProfileManager::GetProfileBaseAndData(ProfileInfo user, ProfileBase& profile,
                                           std::array<u8, MAX_DATA>& data) {
    return GetProfileBaseAndData(user.user_uuid, profile, data);
}

}; // namespace Service::Account
