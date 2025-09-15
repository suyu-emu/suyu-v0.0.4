// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <type_traits>
#include <cstddef>

#include <catch2/catch_test_macros.hpp>
#include "common/common_types.h"
#include "core/crypto/sha_util.h"


using namespace Crypto;

TEST_CASE("SHA256Hash typedef", "[crypto]") {
    SECTION("SHA256Hash is correct size") {
        // SHA256Hash should be 32 bytes (0x20)
        static_assert(sizeof(SHA256Hash) == 0x20);
        static_assert(sizeof(SHA256Hash) == 32);

        SHA256Hash hash{};
        REQUIRE(hash.size() == 32);
        REQUIRE(hash.max_size() == 32);
    }

    SECTION("SHA256Hash is array of u8") {
        static_assert(std::is_same_v<SHA256Hash::value_type, u8>);
        static_assert(std::is_same_v<SHA256Hash, std::array<u8, 0x20>>);
    }

    SECTION("SHA256Hash default initialization") {
        SHA256Hash hash{};
        // Default initialized array should be zero-filled
        for (const auto& byte : hash) {
            REQUIRE(byte == 0);
        }
    }

    SECTION("SHA256Hash can be initialized with values") {
        SHA256Hash hash{};
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
        // Test with various invalid lengths - all should return empty hash
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

    SECTION("Valid 64-character hex string conversion") {
        // Test with exactly 64 characters (0x40) - should convert properly
        auto hash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"_HASH;

        // Expected bytes from the hex string
        Crypto::SHA256Hash expected{
            0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
            0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
            0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
            0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
        };

        REQUIRE(hash == expected);
    }

    SECTION("All zeros hex string") {
        auto hash = "0000000000000000000000000000000000000000000000000000000000000000"_HASH;

        Crypto::SHA256Hash expected{};  // All zeros
        REQUIRE(hash == expected);
    }

    SECTION("All ones hex string (lowercase)") {
        auto hash = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"_HASH;

        Crypto::SHA256Hash expected;
        expected.fill(0xff);  // All bytes set to 0xff
        REQUIRE(hash == expected);
    }

    SECTION("All ones hex string (uppercase)") {
        auto hash = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"_HASH;

        Crypto::SHA256Hash expected;
        expected.fill(0xff);  // All bytes set to 0xff
        REQUIRE(hash == expected);
    }

    SECTION("Mixed case hex string") {
        auto hash = "0123456789AbCdEf0123456789aBcDeF0123456789abcdef0123456789ABCDEF"_HASH;

        // Expected bytes from the hex string (case should not matter)
        Crypto::SHA256Hash expected{
            0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
            0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
            0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
            0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
        };

        REQUIRE(hash == expected);
    }

    SECTION("Real SHA256 hash example") {
        // Example of a real SHA256 hash (empty string hash)
        auto hash = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"_HASH;

        Crypto::SHA256Hash expected{
            0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
            0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
            0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
            0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
        };

        REQUIRE(hash == expected);
    }

    SECTION("Invalid hex characters return empty hash") {
        // Test with invalid hex characters - should return empty hash
        auto hash1 = "gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg"_HASH; // 64 'g' chars
        auto hash2 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeg"_HASH; // 'g' at end
        auto hash3 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd@g"_HASH; // '@' and 'g'
        auto hash4 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd z"_HASH; // space and 'z'

        // These should return empty because they contain invalid hex characters
        Crypto::SHA256Hash empty{};
        REQUIRE(hash1 == empty);
        REQUIRE(hash2 == empty);
        REQUIRE(hash3 == empty);
        REQUIRE(hash4 == empty);
    }

    SECTION("Edge case hex characters") {
        // Test edge cases around valid hex character ranges
        auto hash1 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd/0"_HASH; // '/' is before '0'
        auto hash2 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd:0"_HASH; // ':' is after '9'
        auto hash3 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd`0"_HASH; // '`' is before 'a'
        auto hash4 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdg0"_HASH; // 'g' is after 'f'
        auto hash5 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd@0"_HASH; // '@' is before 'A'
        auto hash6 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdG0"_HASH; // 'G' is after 'F'

        // All should return empty because they contain invalid hex characters
        Crypto::SHA256Hash empty{};
        REQUIRE(hash1 == empty);
        REQUIRE(hash2 == empty);
        REQUIRE(hash3 == empty);
        REQUIRE(hash4 == empty);
        REQUIRE(hash5 == empty);
        REQUIRE(hash6 == empty);
    }

    SECTION("Boundary test - exactly 64 characters") {
        // Test that a string of exactly 64 '0' characters works
        auto hash = "0000000000000000000000000000000000000000000000000000000000000000"_HASH;
        Crypto::SHA256Hash expected{};  // All zeros
        REQUIRE(hash == expected);
    }

    SECTION("Array properties and iterators") {
        auto hash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"_HASH;

        // Test that we can iterate over the hash
        size_t count = 0;
        for (const auto& byte : hash) {
            count++;
        }
        REQUIRE(count == 32);
    }
}
