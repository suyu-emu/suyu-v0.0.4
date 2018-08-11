// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/result.h"

namespace Service::Account {
constexpr size_t MAX_USERS = 8;
constexpr size_t MAX_DATA = 128;
static const u128 INVALID_UUID = {0, 0};

struct UUID {
    // UUIDs which are 0 are considered invalid!
    u128 uuid = INVALID_UUID;
    UUID() = default;
    explicit UUID(const u128& id) : uuid{id} {}
    explicit UUID(const u64 lo, const u64 hi) {
        uuid[0] = lo;
        uuid[1] = hi;
    };
    explicit operator bool() const {
        return uuid[0] != INVALID_UUID[0] || uuid[1] != INVALID_UUID[1];
    }

    bool operator==(const UUID& rhs) const {
        return std::tie(uuid[0], uuid[1]) == std::tie(rhs.uuid[0], rhs.uuid[1]);
    }

    bool operator!=(const UUID& rhs) const {
        return !operator==(rhs);
    }

    // TODO(ogniK): Properly generate uuids based on RFC-4122
    const UUID& Generate() {
        uuid[0] = (static_cast<u64>(std::rand()) << 32) | std::rand();
        uuid[1] = (static_cast<u64>(std::rand()) << 32) | std::rand();
        return *this;
    }
    void Invalidate() {
        uuid = INVALID_UUID;
    }
    std::string Format() const {
        return fmt::format("0x{:016X}{:016X}", uuid[1], uuid[0]);
    }
};
static_assert(sizeof(UUID) == 16, "UUID is an invalid size!");

/// This holds general information about a users profile. This is where we store all the information
/// based on a specific user
struct ProfileInfo {
    UUID user_uuid;
    std::array<u8, 0x20> username;
    u64 creation_time;
    std::array<u8, MAX_DATA> data; // TODO(ognik): Work out what this is
    bool is_open;
};

struct ProfileBase {
    UUID user_uuid;
    u64_le timestamp;
    std::array<u8, 0x20> username;

    void Invalidate() {
        user_uuid.Invalidate();
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
    ProfileManager(); // TODO(ogniK): Load from system save
    ResultCode AddUser(ProfileInfo user);
    ResultCode CreateNewUser(UUID uuid, std::array<u8, 0x20>& username);
    ResultCode CreateNewUser(UUID uuid, const std::string& username);
    size_t GetUserIndex(const UUID& uuid) const;
    size_t GetUserIndex(ProfileInfo user) const;
    bool GetProfileBase(size_t index, ProfileBase& profile) const;
    bool GetProfileBase(UUID uuid, ProfileBase& profile) const;
    bool GetProfileBase(ProfileInfo user, ProfileBase& profile) const;
    bool GetProfileBaseAndData(size_t index, ProfileBase& profile, std::array<u8, MAX_DATA>& data);
    bool GetProfileBaseAndData(UUID uuid, ProfileBase& profile, std::array<u8, MAX_DATA>& data);
    bool GetProfileBaseAndData(ProfileInfo user, ProfileBase& profile,
                               std::array<u8, MAX_DATA>& data);
    size_t GetUserCount() const;
    size_t GetOpenUserCount() const;
    bool UserExists(UUID uuid) const;
    void OpenUser(UUID uuid);
    void CloseUser(UUID uuid);
    std::array<UUID, MAX_USERS> GetOpenUsers() const;
    std::array<UUID, MAX_USERS> GetAllUsers() const;
    UUID GetLastOpenedUser() const;

    bool CanSystemRegisterUser() const;

private:
    std::array<ProfileInfo, MAX_USERS> profiles{};
    size_t user_count = 0;
    size_t AddToProfiles(const ProfileInfo& profile);
    bool RemoveProfileAtIdx(size_t index);
    UUID last_opened_user{0, 0};
};
using ProfileManagerPtr = std::unique_ptr<ProfileManager>;

}; // namespace Service::Account
