// SPDX-FileCopyrightText: 2025 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdexcept>
#include <catch2/catch_test_macros.hpp>
#include "common/assert.h"
#include "common/settings.h"

namespace {

// Mock implementation to avoid actual crashes during testing
bool crash_called = false;
bool log_stop_called = false;

// Override the Crash() macro for testing
#ifdef Crash
#undef Crash
#endif
#define Crash() crash_called = true

// Mock Common::Log::Stop for testing
namespace Common::Log {
void Stop() {
    log_stop_called = true;
}
} // namespace Common::Log

// Reset mock state before each test
void ResetMockState() {
    crash_called = false;
    log_stop_called = false;
}

} // namespace

TEST_CASE("assert_fail_impl", "[common][assert]") {
    SECTION("when use_debug_asserts is true") {
        ResetMockState();
        Settings::values.use_debug_asserts = true;

        assert_fail_impl();

        REQUIRE(log_stop_called == true);
        REQUIRE(crash_called == true);
    }

    SECTION("when use_debug_asserts is false") {
        ResetMockState();
        Settings::values.use_debug_asserts = false;

        assert_fail_impl();

        REQUIRE(log_stop_called == false);
        REQUIRE(crash_called == false);
    }
}

TEST_CASE("unreachable_impl", "[common][assert]") {
    SECTION("always stops logging and crashes") {
        ResetMockState();

        REQUIRE_THROWS_AS(unreachable_impl(), std::runtime_error);

        REQUIRE(log_stop_called == true);
        REQUIRE(crash_called == true);
    }

    SECTION("throws runtime_error with correct message") {
        ResetMockState();

        REQUIRE_THROWS_WITH(unreachable_impl(), "Unreachable code");
    }
}

TEST_CASE("ASSERT macro behavior", "[common][assert]") {
    SECTION("does not trigger when condition is true") {
        ResetMockState();
        Settings::values.use_debug_asserts = true;

        ASSERT(true);

        REQUIRE(log_stop_called == false);
        REQUIRE(crash_called == false);
    }

    SECTION("triggers when condition is false and use_debug_asserts is true") {
        ResetMockState();
        Settings::values.use_debug_asserts = true;

        ASSERT(false);

        REQUIRE(log_stop_called == true);
        REQUIRE(crash_called == true);
    }

    SECTION("does not trigger when condition is false but use_debug_asserts is false") {
        ResetMockState();
        Settings::values.use_debug_asserts = false;

        ASSERT(false);

        REQUIRE(log_stop_called == false);
        REQUIRE(crash_called == false);
    }
}

TEST_CASE("UNREACHABLE macro behavior", "[common][assert]") {
    SECTION("always triggers unreachable_impl") {
        ResetMockState();

        REQUIRE_THROWS_AS(UNREACHABLE(), std::runtime_error);

        REQUIRE(log_stop_called == true);
        REQUIRE(crash_called == true);
    }

    SECTION("throws with correct message") {
        REQUIRE_THROWS_WITH(UNREACHABLE(), "Unreachable code");
    }
}
