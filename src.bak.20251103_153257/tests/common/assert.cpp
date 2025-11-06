// SPDX-FileCopyrightText: 2025 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdexcept>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h"
#include "common/assert.h"

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

TEST_CASE("assert_fail_impl behavior when debug asserts disabled", "[common][assert]") {
    SaveOriginalSettings();

    SECTION("when use_debug_asserts is false") {
        Settings::values.use_debug_asserts.SetValue(false);

        // When use_debug_asserts is false, assert_fail_impl should return without crashing
        REQUIRE_NOTHROW(assert_fail_impl());
    }

    RestoreOriginalSettings();
}

TEST_CASE("assert_fail_impl behavior when debug asserts enabled", "[common][assert]") {
    SaveOriginalSettings();

    SECTION("when use_debug_asserts is true") {
        Settings::values.use_debug_asserts.SetValue(true);

        // When use_debug_asserts is true, assert_fail_impl calls Crash() which terminates the process.
        // We cannot directly test this in a unit test as it would kill the test runner.
        // However, we can verify that the function exists and the setting is properly read.
        // The actual crash behavior would need to be tested through integration tests.

        // Verify the setting is correctly set
        REQUIRE(Settings::values.use_debug_asserts.GetValue() == true);

        // Note: We cannot call assert_fail_impl() here as it would crash the test process
        // This is expected behavior - the function is designed to terminate the program
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

    SECTION("function is marked as noreturn") {
        // Test that the function consistently throws and never returns
        bool exception_thrown = false;
        try {
            unreachable_impl();
        } catch (const std::runtime_error&) {
            exception_thrown = true;
        }
        REQUIRE(exception_thrown == true);
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

    SECTION("assert_fail_impl respects use_debug_asserts setting when disabled") {
        // Test with debug asserts disabled - should not crash
        Settings::values.use_debug_asserts.SetValue(false);
        REQUIRE_NOTHROW(assert_fail_impl());
    }

    RestoreOriginalSettings();
}

TEST_CASE("Assert macros compilation and basic behavior", "[common][assert]") {
    SaveOriginalSettings();

    SECTION("ASSERT macro compiles and works with debug asserts disabled") {
        Settings::values.use_debug_asserts.SetValue(false);

        // These should compile and not crash when debug asserts are disabled
        REQUIRE_NOTHROW(ASSERT(true));
        REQUIRE_NOTHROW(ASSERT(false)); // Should not crash when debug asserts are off
    }

    SECTION("ASSERT_MSG macro compiles and works with debug asserts disabled") {
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

    SECTION("UNIMPLEMENTED_IF macro behavior") {
        Settings::values.use_debug_asserts.SetValue(false);

        // UNIMPLEMENTED_IF should not crash when debug asserts are disabled
        REQUIRE_NOTHROW(UNIMPLEMENTED_IF(true));  // Should trigger but not crash
        REQUIRE_NOTHROW(UNIMPLEMENTED_IF(false)); // Should not trigger
    }

    SECTION("UNIMPLEMENTED_IF_MSG macro behavior") {
        Settings::values.use_debug_asserts.SetValue(false);

        // UNIMPLEMENTED_IF_MSG should not crash when debug asserts are disabled
        REQUIRE_NOTHROW(UNIMPLEMENTED_IF_MSG(true, "Condition is true"));  // Should trigger but not crash
        REQUIRE_NOTHROW(UNIMPLEMENTED_IF_MSG(false, "Condition is false")); // Should not trigger
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

    SECTION("ASSERT_OR_EXECUTE_MSG macro behavior") {
        Settings::values.use_debug_asserts.SetValue(false);
        bool executed = false;

        // When condition is false, the execution block should run
        ASSERT_OR_EXECUTE_MSG(false, executed = true;, "Condition failed");
        REQUIRE(executed == true);

        // When condition is true, the execution block should not run
        executed = false;
        ASSERT_OR_EXECUTE_MSG(true, executed = true;, "This should not execute");
        REQUIRE(executed == false);
    }

    RestoreOriginalSettings();
}

#ifdef _DEBUG
TEST_CASE("Debug assert macros in debug builds", "[common][assert]") {
    SaveOriginalSettings();

    SECTION("DEBUG_ASSERT macro behavior in debug builds") {
        Settings::values.use_debug_asserts.SetValue(false);

        // In debug builds, DEBUG_ASSERT should behave like ASSERT
        REQUIRE_NOTHROW(DEBUG_ASSERT(true));
        REQUIRE_NOTHROW(DEBUG_ASSERT(false)); // Should not crash when debug asserts are off
    }

    SECTION("DEBUG_ASSERT_MSG macro behavior in debug builds") {
        Settings::values.use_debug_asserts.SetValue(false);

        // In debug builds, DEBUG_ASSERT_MSG should behave like ASSERT_MSG
        REQUIRE_NOTHROW(DEBUG_ASSERT_MSG(true, "This should not trigger"));
        REQUIRE_NOTHROW(DEBUG_ASSERT_MSG(false, "This should not crash when debug asserts are off"));
    }

    RestoreOriginalSettings();
}
#else
TEST_CASE("Debug assert macros in release builds", "[common][assert]") {
    SECTION("DEBUG_ASSERT macro is no-op in release builds") {
        // In release builds, DEBUG_ASSERT should be a no-op
        REQUIRE_NOTHROW(DEBUG_ASSERT(true));
        REQUIRE_NOTHROW(DEBUG_ASSERT(false)); // Should always be no-op in release
    }

    SECTION("DEBUG_ASSERT_MSG macro is no-op in release builds") {
        // In release builds, DEBUG_ASSERT_MSG should be a no-op
        REQUIRE_NOTHROW(DEBUG_ASSERT_MSG(true, "This should not trigger"));
        REQUIRE_NOTHROW(DEBUG_ASSERT_MSG(false, "This should be no-op in release"));
    }
}
#endif

TEST_CASE("Macro edge cases and complex scenarios", "[common][assert]") {
    SaveOriginalSettings();
    Settings::values.use_debug_asserts.SetValue(false);

    SECTION("Multiple ASSERT_OR_EXECUTE calls") {
        int counter = 0;

        // Test multiple calls with different conditions
        ASSERT_OR_EXECUTE(false, counter++;);
        ASSERT_OR_EXECUTE(true, counter++;);
        ASSERT_OR_EXECUTE(false, counter++;);

        REQUIRE(counter == 2); // Only false conditions should execute
    }

    SECTION("Complex expressions in ASSERT") {
        int value = 5;

        // Test with complex boolean expressions
        REQUIRE_NOTHROW(ASSERT(value > 0 && value < 10));
        REQUIRE_NOTHROW(ASSERT(value < 0 || value > 10)); // Should not crash when debug asserts are off
    }

    SECTION("ASSERT_OR_EXECUTE with complex execution blocks") {
        int result = 0;

        ASSERT_OR_EXECUTE(false, {
            result = 42;
            result *= 2;
        });

        REQUIRE(result == 84);
    }

    SECTION("Nested macro usage") {
        bool outer_executed = false;
        bool inner_executed = false;

        ASSERT_OR_EXECUTE(false, {
            outer_executed = true;
            ASSERT_OR_EXECUTE(false, inner_executed = true;);
        });

        REQUIRE(outer_executed == true);
        REQUIRE(inner_executed == true);
    }

    RestoreOriginalSettings();
}

TEST_CASE("Function behavior consistency", "[common][assert]") {
    SaveOriginalSettings();

    SECTION("assert_fail_impl consistency across multiple calls when disabled") {
        Settings::values.use_debug_asserts.SetValue(false);

        // Multiple calls should behave consistently
        REQUIRE_NOTHROW(assert_fail_impl());
        REQUIRE_NOTHROW(assert_fail_impl());
        REQUIRE_NOTHROW(assert_fail_impl());
    }

    SECTION("unreachable_impl always throws consistently") {
        // Multiple calls should always throw
        REQUIRE_THROWS_AS(unreachable_impl(), std::runtime_error);
        REQUIRE_THROWS_AS(unreachable_impl(), std::runtime_error);
        REQUIRE_THROWS_AS(unreachable_impl(), std::runtime_error);
    }

    RestoreOriginalSettings();
}

TEST_CASE("Assert macro parameter evaluation", "[common][assert]") {
    SaveOriginalSettings();
    Settings::values.use_debug_asserts.SetValue(false);

    SECTION("ASSERT evaluates condition only once") {
        int call_count = 0;
        auto condition = [&call_count]() -> bool {
            call_count++;
            return true;
        };

        ASSERT(condition());
        REQUIRE(call_count == 1);
    }

    SECTION("ASSERT_OR_EXECUTE evaluates condition correctly") {
        int condition_calls = 0;
        int execution_calls = 0;

        auto condition = [&condition_calls]() -> bool {
            condition_calls++;
            return false;
        };

        ASSERT_OR_EXECUTE(condition(), execution_calls++;);

        // Condition should be evaluated at least once for the ASSERT and once for the if check
        REQUIRE(condition_calls >= 1);
        REQUIRE(execution_calls == 1);
    }

    RestoreOriginalSettings();
}

TEST_CASE("Exception safety and resource management", "[common][assert]") {
    SECTION("unreachable_impl exception is properly typed") {
        try {
            unreachable_impl();
            FAIL("unreachable_impl should have thrown an exception");
        } catch (const std::runtime_error& e) {
            REQUIRE(std::string(e.what()) == "Unreachable code");
        } catch (...) {
            FAIL("unreachable_impl should throw std::runtime_error, not other exception types");
        }
    }

    SECTION("Settings state is preserved across test sections") {
        SaveOriginalSettings();

        // Modify settings
        Settings::values.use_debug_asserts.SetValue(!original_use_debug_asserts);
        REQUIRE(Settings::values.use_debug_asserts.GetValue() != original_use_debug_asserts);

        // Restore settings
        RestoreOriginalSettings();
        REQUIRE(Settings::values.use_debug_asserts.GetValue() == original_use_debug_asserts);
    }
}
