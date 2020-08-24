// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdlib>
#include "common/common_funcs.h"
#include "common/logging/log.h"

// For asserts we'd like to keep all the junk executed when an assert happens away from the
// important code in the function. One way of doing this is to put all the relevant code inside a
// lambda and force the compiler to not inline it. Unfortunately, MSVC seems to have no syntax to
// specify __declspec on lambda functions, so what we do instead is define a noinline wrapper
// template that calls the lambda. This seems to generate an extra instruction at the call-site
// compared to the ideal implementation (which wouldn't support ASSERT_MSG parameters), but is good
// enough for our purposes.
template <typename Fn>
#if defined(_MSC_VER)
[[msvc::noinline, noreturn]]
#elif defined(__GNUC__)
[[gnu::cold, gnu::noinline, noreturn]]
#endif
static void
assert_noinline_call(const Fn& fn) {
    fn();
    Crash();
    exit(1); // Keeps GCC's mouth shut about this actually returning
}

#define ASSERT(_a_)                                                                                \
    do                                                                                             \
        if (!(_a_)) {                                                                              \
            assert_noinline_call([] { LOG_CRITICAL(Debug, "Assertion Failed!"); });                \
        }                                                                                          \
    while (0)

#define ASSERT_MSG(_a_, ...)                                                                       \
    do                                                                                             \
        if (!(_a_)) {                                                                              \
            assert_noinline_call([&] { LOG_CRITICAL(Debug, "Assertion Failed!\n" __VA_ARGS__); }); \
        }                                                                                          \
    while (0)

#define UNREACHABLE() assert_noinline_call([] { LOG_CRITICAL(Debug, "Unreachable code!"); })
#define UNREACHABLE_MSG(...)                                                                       \
    assert_noinline_call([&] { LOG_CRITICAL(Debug, "Unreachable code!\n" __VA_ARGS__); })

#ifdef _DEBUG
#define DEBUG_ASSERT(_a_) ASSERT(_a_)
#define DEBUG_ASSERT_MSG(_a_, ...) ASSERT_MSG(_a_, __VA_ARGS__)
#else // not debug
#define DEBUG_ASSERT(_a_)
#define DEBUG_ASSERT_MSG(_a_, _desc_, ...)
#endif

#define UNIMPLEMENTED() ASSERT_MSG(false, "Unimplemented code!")
#define UNIMPLEMENTED_MSG(...) ASSERT_MSG(false, __VA_ARGS__)

#define UNIMPLEMENTED_IF(cond) ASSERT_MSG(!(cond), "Unimplemented code!")
#define UNIMPLEMENTED_IF_MSG(cond, ...) ASSERT_MSG(!(cond), __VA_ARGS__)

// If the assert is ignored, execute _b_
#define ASSERT_OR_EXECUTE(_a_, _b_)                                                                \
    do {                                                                                           \
        ASSERT(_a_);                                                                               \
        if (!(_a_)) {                                                                              \
            _b_                                                                                    \
        }                                                                                          \
    } while (0)

// If the assert is ignored, execute _b_
#define ASSERT_OR_EXECUTE_MSG(_a_, _b_, ...)                                                       \
    do {                                                                                           \
        ASSERT_MSG(_a_, __VA_ARGS__);                                                              \
        if (!(_a_)) {                                                                              \
            _b_                                                                                    \
        }                                                                                          \
    } while (0)
