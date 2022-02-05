// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <optional>

#include "common/common_types.h"
#include "common/new_uuid.h"
#include "common/swap.h"
#include "core/hle/result.h"

namespace Service::Account {

constexpr std::size_t MAX_USERS{8};
constexpr std::size_t profile_username_size{32};

using ProfileUsername = std::array<u8, profile_username_size>;
using UserIDArray = std::array<Common::NewUUID, MAX_USERS>;

/// Contains extra data related to a user.
/// TODO: RE this structure
struct ProfileData {
    INSERT_PADDING_WORDS_NOINIT(1);
    u32 icon_id;
    u8 bg_color_id;
    INSERT_PADDING_BYTES_NOINIT(0x7);
    INSERT_PADDING_BYTES_NOINIT(0x10);
    INSERT_PADDING_BYTES_NOINIT(0x60);
};
static_assert(sizeof(ProfileData) == 0x80, "ProfileData structure has incorrect size");

/// This holds general information about a users profile. This is where we store all the information
/// based on a specific user
struct ProfileInfo {
    Common::NewUUID user_uuid{};
    ProfileUsername username{};
    u64 creation_time{};
    ProfileData data{}; // TODO(ognik): Work out what this is
    bool is_open{};
};

struct ProfileBase {
    Common::NewUUID user_uuid;
    u64_le timestamp;
    ProfileUsername username;

    // Zero out all the fields to make the profile slot considered "Empty"
    void Invalidate() {
        user_uuid = {};
        timestamp = 0;
        username.fill(0);
    }
};
static_assert(sizeof(ProfileBase) == 0x38, "ProfileBase is an invalid size");

/// The profile manager is used for handling multiple user profiles at once. It keeps track of open
/// users, all the accounts registered on the "system" as well as fetching individual "ProfileInfo"
/// objects
class ProfileManager {
public:
    ProfileManager();
    ~ProfileManager();

    ResultCode AddUser(const ProfileInfo& user);
    ResultCode CreateNewUser(Common::NewUUID uuid, const ProfileUsername& username);
    ResultCode CreateNewUser(Common::NewUUID uuid, const std::string& username);
    std::optional<Common::NewUUID> GetUser(std::size_t index) const;
    std::optional<std::size_t> GetUserIndex(const Common::NewUUID& uuid) const;
    std::optional<std::size_t> GetUserIndex(const ProfileInfo& user) const;
    bool GetProfileBase(std::optional<std::size_t> index, ProfileBase& profile) const;
    bool GetProfileBase(Common::NewUUID uuid, ProfileBase& profile) const;
    bool GetProfileBase(const ProfileInfo& user, ProfileBase& profile) const;
    bool GetProfileBaseAndData(std::optional<std::size_t> index, ProfileBase& profile,
                               ProfileData& data) const;
    bool GetProfileBaseAndData(Common::NewUUID uuid, ProfileBase& profile, ProfileData& data) const;
    bool GetProfileBaseAndData(const ProfileInfo& user, ProfileBase& profile,
                               ProfileData& data) const;
    std::size_t GetUserCount() const;
    std::size_t GetOpenUserCount() const;
    bool UserExists(Common::NewUUID uuid) const;
    bool UserExistsIndex(std::size_t index) const;
    void OpenUser(Common::NewUUID uuid);
    void CloseUser(Common::NewUUID uuid);
    UserIDArray GetOpenUsers() const;
    UserIDArray GetAllUsers() const;
    Common::NewUUID GetLastOpenedUser() const;

    bool CanSystemRegisterUser() const;

    bool RemoveUser(Common::NewUUID uuid);
    bool SetProfileBase(Common::NewUUID uuid, const ProfileBase& profile_new);
    bool SetProfileBaseAndData(Common::NewUUID uuid, const ProfileBase& profile_new,
                               const ProfileData& data_new);

private:
    void ParseUserSaveFile();
    void WriteUserSaveFile();
    std::optional<std::size_t> AddToProfiles(const ProfileInfo& profile);
    bool RemoveProfileAtIndex(std::size_t index);

    std::array<ProfileInfo, MAX_USERS> profiles{};
    std::size_t user_count{};
    Common::NewUUID last_opened_user{};
};

}; // namespace Service::Account
