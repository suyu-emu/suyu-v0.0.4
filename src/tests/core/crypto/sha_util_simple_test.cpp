// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>
#include "core/crypto/sha_util.h"

TEST_CASE("SHA256Hash basic functionality", "[crypto][simple]") {
    SECTION("Basic hash creation") {
        // Test that we can create a hash from a valid hex string
        auto hash = "0000000000000000000000000000000000000000000000000000000000000000"_HASH;

        // Verify it's all zeros
        for (const auto& byte : hash) {
            REQUIRE(byte == 0);
        }
    }

    SECTION("Invalid length handling") {
        // Test that invalid lengths return empty hash
        auto hash = "invalid"_HASH;
        Crypto::SHA256Hash empty{};
        REQUIRE(hash == empty);
    }
}
