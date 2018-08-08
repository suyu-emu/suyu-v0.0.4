#pragma once
#include <array>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/result.h"

namespace Service::Account {
constexpr size_t MAX_USERS = 8;
constexpr size_t MAX_DATA = 128;

struct UUID {
    // UUIDs which are 0 are considered invalid!
    u128 uuid{0, 0};
    UUID() = default;
    explicit UUID(const u128& id) {
        uuid[0] = id[0];
        uuid[1] = id[1];
    };
    explicit UUID(const u64& lo, const u64& hi) {
        uuid[0] = lo;
        uuid[1] = hi;
    };
    operator bool() const {
        return uuid[0] != 0x0 && uuid[1] != 0x0;
    }

    bool operator==(const UUID& rhs) {
        return uuid[0] == rhs.uuid[0] && uuid[1] == rhs.uuid[1];
    }

    bool operator!=(const UUID& rhs) {
        return uuid[0] != rhs.uuid[0] || uuid[1] != rhs.uuid[1];
    }

    // TODO(ogniK): Properly generate uuids based on RFC-4122
    const UUID& Generate() {
        uuid[0] = (static_cast<u64>(std::rand()) << 32) | std::rand();
        uuid[1] = (static_cast<u64>(std::rand()) << 32) | std::rand();
        return *this;
    }
    void Invalidate() {
        uuid[0] = 0;
        uuid[1] = 0;
    }
    std::string Format() {
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
    std::array<u8, MAX_DATA> data;
};

struct ProfileBase {
    UUID user_uuid;
    u64_le timestamp;
    std::array<u8, 0x20> username;

    const void Invalidate() {
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
    ProfileManager() = default; // TODO(ogniK): Load from system save
    ResultCode AddUser(ProfileInfo user);
    ResultCode CreateNewUser(UUID uuid, std::array<u8, 0x20> username);
    size_t GetUserIndex(UUID uuid);
    size_t GetUserIndex(ProfileInfo user);
    bool GetProfileBase(size_t index, ProfileBase& profile);
    bool GetProfileBase(UUID uuid, ProfileBase& profile);
    bool GetProfileBase(ProfileInfo user, ProfileBase& profile);
    size_t GetUserCount();
    bool UserExists(UUID uuid);

private:
    std::array<ProfileInfo, MAX_USERS> profiles{};
    size_t user_count = 0;
    size_t AddToProfiles(const ProfileInfo& profile);
    bool RemoveProfileAtIdx(size_t index);
};
using ProfileManagerPtr = std::unique_ptr<ProfileManager>;

}; // namespace Service::Account
