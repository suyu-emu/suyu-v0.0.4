// Copyright 2022 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <string>
#include <string_view>

#include "common/common_types.h"

namespace Common {

struct NewUUID {
    std::array<u8, 0x10> uuid{};

    /// Constructs an invalid UUID.
    constexpr NewUUID() = default;

    /// Constructs a UUID from a reference to a 128 bit array.
    constexpr explicit NewUUID(const std::array<u8, 16>& uuid_) : uuid{uuid_} {}

    /**
     * Constructs a UUID from either:
     * 1. A 32 hexadecimal character string representing the bytes of the UUID
     * 2. A RFC 4122 formatted UUID string, in the format xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
     *
     * The input string may contain uppercase or lowercase characters, but they must:
     * 1. Contain valid hexadecimal characters (0-9, a-f, A-F)
     * 2. Not contain the "0x" hexadecimal prefix
     *
     * Should the input string not meet the above requirements,
     * an assert will be triggered and an invalid UUID is set instead.
     */
    explicit NewUUID(std::string_view uuid_string);

    ~NewUUID() = default;

    constexpr NewUUID(const NewUUID&) noexcept = default;
    constexpr NewUUID(NewUUID&&) noexcept = default;

    constexpr NewUUID& operator=(const NewUUID&) noexcept = default;
    constexpr NewUUID& operator=(NewUUID&&) noexcept = default;

    /**
     * Returns whether the stored UUID is valid or not.
     *
     * @returns True if the stored UUID is valid, false otherwise.
     */
    constexpr bool IsValid() const {
        return uuid != std::array<u8, 0x10>{};
    }

    /**
     * Returns whether the stored UUID is invalid or not.
     *
     * @returns True if the stored UUID is invalid, false otherwise.
     */
    constexpr bool IsInvalid() const {
        return !IsValid();
    }

    /**
     * Returns a 32 hexadecimal character string representing the bytes of the UUID.
     *
     * @returns A 32 hexadecimal character string of the UUID.
     */
    std::string RawString() const;

    /**
     * Returns a RFC 4122 formatted UUID string in the format xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx.
     *
     * @returns A RFC 4122 formatted UUID string.
     */
    std::string FormattedString() const;

    /**
     * Returns a 64-bit hash of the UUID for use in hash table data structures.
     *
     * @returns A 64-bit hash of the UUID.
     */
    size_t Hash() const noexcept;

    /// DO NOT USE. Copies the contents of the UUID into a u128.
    u128 AsU128() const;

    /**
     * Creates a default UUID "yuzu Default UID".
     *
     * @returns A UUID with its bytes set to the ASCII values of "yuzu Default UID".
     */
    static constexpr NewUUID MakeDefault() {
        return NewUUID{
            {'y', 'u', 'z', 'u', ' ', 'D', 'e', 'f', 'a', 'u', 'l', 't', ' ', 'U', 'I', 'D'},
        };
    }

    /**
     * Creates a random UUID.
     *
     * @returns A random UUID.
     */
    static NewUUID MakeRandom();

    /**
     * Creates a random UUID with a seed.
     *
     * @param seed A seed to initialize the Mersenne-Twister RNG
     *
     * @returns A random UUID.
     */
    static NewUUID MakeRandomWithSeed(u32 seed);

    /**
     * Creates a random UUID. The generated UUID is RFC 4122 Version 4 compliant.
     *
     * @returns A random UUID that is RFC 4122 Version 4 compliant.
     */
    static NewUUID MakeRandomRFC4122V4();

    friend constexpr bool operator==(const NewUUID& lhs, const NewUUID& rhs) = default;
};
static_assert(sizeof(NewUUID) == 0x10, "UUID has incorrect size.");

/// An invalid UUID. This UUID has all its bytes set to 0.
constexpr NewUUID InvalidUUID = {};

} // namespace Common

namespace std {

template <>
struct hash<Common::NewUUID> {
    size_t operator()(const Common::NewUUID& uuid) const noexcept {
        return uuid.Hash();
    }
};

} // namespace std
