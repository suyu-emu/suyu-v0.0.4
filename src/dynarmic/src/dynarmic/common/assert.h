// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2013 Dolphin Emulator Project
// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// TODO: Use source_info?
[[noreturn]] void assert_terminate_impl(const char* s);
#ifndef ASSERT
#   define ASSERT(expr) do if(!(expr)) [[unlikely]] assert_terminate_impl(__FILE__ ": " #expr); while(0)
#endif
#ifndef UNREACHABLE
#   ifdef _MSC_VER
#       define UNREACHABLE() ASSERT(false && __FILE__ ": unreachable")
#   else
#       define UNREACHABLE() __builtin_unreachable();
#   endif
#endif
#ifndef DEBUG_ASSERT
#   ifndef NDEBUG
#       define DEBUG_ASSERT(_a_) ASSERT(_a_)
#   else
#       define DEBUG_ASSERT(_a_)
#   endif
#endif
