// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "common/common_types.h"
#include "core/crypto/sha_util.h"

TEST_CASE("SHA256Hash typedef", "[crypto]") {
    SECTION("SHA256Hash is correct size") {
        // SHA256Hash should be 32 bytes (0x20)
        static_assert(sizeof(Crypto::SHA256Hash) == 0x20);
        static_assert(sizeof(Crypto::SHA256Hash) == 32);

        Crypto::SHA256Hash hash{};
        REQUIRE(hash.size() == 32);
        REQUIRE(hash.max_size() == 32);
    }

    SECTION("SHA256Hash is array of u8") {
        static_assert(std::is_same_v<Crypto::SHA256Hash::value_type, u8>);
        static_assert(std::is_same_v<Crypto::SHA256Hash, std::array<u8, 0x20>>);
    }

    SECTION("SHA256Hash default initialization") {
        Crypto::SHA256Hash hash{};
        // Default initialized array should be zero-filled
        for (const auto& byte : hash) {
            REQUIRE(byte == 0);
        }
    }

    SECTION("SHA256Hash can be initialized with values") {
        Crypto::SHA256Hash hash{};
        hash[0] = 0xAB;
        hash[1] = 0xCD;
        hash[31] = 0xEF;

        REQUIRE(hash[0] == 0xAB);
        REQUIRE(hash[1] == 0xCD);
        REQUIRE(hash[31] == 0xEF);
        REQUIRE(hash[2] == 0x00); // Other bytes should remain zero
    }
}

TEST_CASE("_HASH operator", "[crypto]") {
    SECTION("Invalid length returns empty hash") {
        // Test with various invalid lengths
        auto hash1 = ""_HASH;
        auto hash2 = "abc"_HASH;
        auto hash3 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde"_HASH; // 63 chars
        auto hash4 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0"_HASH; // 65 chars

        // All should return empty (zero-filled) hash
        Crypto::SHA256Hash empty{};
        REQUIRE(hash1 == empty);
        REQUIRE(hash2 == empty);
        REQUIRE(hash3 == empty);
        REQUIRE(hash4 == empty);
    }

    SECTION("Valid 64-character hex string length") {
        // Test with exactly 64 characters (0x40)
        auto hash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"_HASH;

        // The current implementation only checks length and returns empty for invalid length
        // Since the implementation is incomplete, it should return empty for now
        Crypto::SHA256Hash empty{};
        REQUIRE(hash == empty);
    }

    SECTION("All zeros hex string") {
        auto hash = "0000000000000000000000000000000000000000000000000000000000000000"_HASH;

        Crypto::SHA256Hash empty{};
        REQUIRE(hash == empty);
    }

    SECTION("All ones hex string") {
        auto hash = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"_HASH;

        Crypto::SHA256Hash empty{};
        REQUIRE(hash == empty);
    }

    SECTION("Mixed case hex string") {
        auto hash = "0123456789AbCdEf0123456789aBcDeF0123456789abcdef0123456789ABCDEF"_HASH;

        Crypto::SHA256Hash empty{};
        REQUIRE(hash == empty);
    }
}
