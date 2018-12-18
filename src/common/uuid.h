// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Common {

constexpr u128 INVALID_UUID{{0, 0}};

struct UUID {
    // UUIDs which are 0 are considered invalid!
    u128 uuid = INVALID_UUID;
    constexpr UUID() = default;
    constexpr explicit UUID(const u128& id) : uuid{id} {}
    constexpr explicit UUID(const u64 lo, const u64 hi) : uuid{{lo, hi}} {}

    explicit operator bool() const {
        return uuid != INVALID_UUID;
    }

    bool operator==(const UUID& rhs) const {
        return uuid == rhs.uuid;
    }

    bool operator!=(const UUID& rhs) const {
        return !operator==(rhs);
    }

    // TODO(ogniK): Properly generate uuids based on RFC-4122
    static UUID Generate();

    // Set the UUID to {0,0} to be considered an invalid user
    void Invalidate() {
        uuid = INVALID_UUID;
    }

    std::string Format() const;
    std::string FormatSwitch() const;
};
static_assert(sizeof(UUID) == 16, "UUID is an invalid size!");

} // namespace Common
