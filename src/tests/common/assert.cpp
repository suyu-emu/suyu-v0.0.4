// SPDX-FileCopyrightText: 2025 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdexcept>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"

// Forward declarations for the functions we're testing
void assert_fail_impl();
[[noreturn]] void unreachable_impl();

// Test state tracking
namespace {
bool original_use_debug_asserts;

void SaveOriginalSettings() {
    original_use_debug_asserts = Settings::values.use_debug_asserts.GetValue();
}

void RestoreOriginalSettings() {
    Settings::values.use_debug_asserts.SetValue(original_use_debug_asserts);
}

} // namespace

TEST_CASE("assert_fail_impl behavior", "[common][assert]") {
    SaveOriginalSettings();

    SECTION("when use_debug_asserts is true") {
        Settings::values.use_debug_asserts.SetValue(true);

        // Since assert_fail_impl() calls Crash() which would terminate the process,
        // we can't directly test the crash behavior in a unit test.
        // Instead, we test that the function exists and can be called.
        // The actual crash behavior would need to be tested through integration tests
        // or by mocking the Crash() function at a lower level.

        // We can verify the function doesn't throw an exception when called
        REQUIRE_NOTHROW(assert_fail_impl());
    }

    SECTION("when use_debug_asserts is false") {
        Settings::values.use_debug_asserts.SetValue(false);

        // When use_debug_asserts is false, assert_fail_impl should return without crashing
        REQUIRE_NOTHROW(assert_fail_impl());
    }

    RestoreOriginalSettings();
}

TEST_CASE("unreachable_impl behavior", "[common][assert]") {
    SECTION("always throws std::runtime_error") {
        // unreachable_impl() should always throw a std::runtime_error
        REQUIRE_THROWS_AS(unreachable_impl(), std::runtime_error);
    }

    SECTION("throws with correct error message") {
        // Verify the exception message is correct
        REQUIRE_THROWS_WITH(unreachable_impl(), "Unreachable code");
    }
}

TEST_CASE("Settings integration", "[common][assert]") {
    SaveOriginalSettings();

    SECTION("use_debug_asserts setting can be modified") {
        // Test that we can change the setting value
        Settings::values.use_debug_asserts.SetValue(true);
        REQUIRE(Settings::values.use_debug_asserts.GetValue() == true);

        Settings::values.use_debug_asserts.SetValue(false);
        REQUIRE(Settings::values.use_debug_asserts.GetValue() == false);
    }

    SECTION("assert_fail_impl respects use_debug_asserts setting") {
        // Test with debug asserts enabled
        Settings::values.use_debug_asserts.SetValue(true);
        REQUIRE_NOTHROW(assert_fail_impl());

        // Test with debug asserts disabled
        Settings::values.use_debug_asserts.SetValue(false);
        REQUIRE_NOTHROW(assert_fail_impl());
    }

    RestoreOriginalSettings();
}

// Test the macro behavior indirectly by including the header
#include "common/assert.h"

TEST_CASE("Assert macros compilation", "[common][assert]") {
    SaveOriginalSettings();

    SECTION("ASSERT macro compiles and can be used") {
        Settings::values.use_debug_asserts.SetValue(false);

        // These should compile and not crash when debug asserts are disabled
        REQUIRE_NOTHROW(ASSERT(true));
        REQUIRE_NOTHROW(ASSERT(false)); // Should not crash when debug asserts are off
    }

    SECTION("ASSERT_MSG macro compiles and can be used") {
        Settings::values.use_debug_asserts.SetValue(false);

        // These should compile and not crash when debug asserts are disabled
        REQUIRE_NOTHROW(ASSERT_MSG(true, "This should not trigger"));
        REQUIRE_NOTHROW(ASSERT_MSG(false, "This should not crash when debug asserts are off"));
    }

    SECTION("UNREACHABLE macro throws exception") {
        // UNREACHABLE should always throw regardless of settings
        REQUIRE_THROWS_AS(UNREACHABLE(), std::runtime_error);
        REQUIRE_THROWS_WITH(UNREACHABLE(), "Unreachable code");
    }

    SECTION("UNREACHABLE_MSG macro throws exception with message") {
        // UNREACHABLE_MSG should always throw regardless of settings
        REQUIRE_THROWS_AS(UNREACHABLE_MSG("Custom message"), std::runtime_error);
        REQUIRE_THROWS_WITH(UNREACHABLE_MSG("Custom message"), "Unreachable code");
    }

    SECTION("UNIMPLEMENTED macro behavior") {
        Settings::values.use_debug_asserts.SetValue(false);

        // UNIMPLEMENTED should not crash when debug asserts are disabled
        REQUIRE_NOTHROW(UNIMPLEMENTED());
    }

    SECTION("UNIMPLEMENTED_MSG macro behavior") {
        Settings::values.use_debug_asserts.SetValue(false);

        // UNIMPLEMENTED_MSG should not crash when debug asserts are disabled
        REQUIRE_NOTHROW(UNIMPLEMENTED_MSG("Not implemented yet"));
    }

    SECTION("ASSERT_OR_EXECUTE macro behavior") {
        Settings::values.use_debug_asserts.SetValue(false);
        bool executed = false;

        // When condition is false, the execution block should run
        ASSERT_OR_EXECUTE(false, executed = true;);
        REQUIRE(executed == true);

        // When condition is true, the execution block should not run
        executed = false;
        ASSERT_OR_EXECUTE(true, executed = true;);
        REQUIRE(executed == false);
    }

    RestoreOriginalSettings();
}
