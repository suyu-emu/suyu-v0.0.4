// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#endif

#include "common/common_funcs.h"

// Generic function to get last error message.
// Call directly after the command or use the error num.
// This function might change the error code.
std::string GetLastErrorMsg() {
    static constexpr std::size_t buff_size = 255;
    char err_str[buff_size];

#ifdef _WIN32
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), err_str, buff_size, nullptr);
    return std::string(err_str, buff_size);
#elif defined(__GLIBC__) && (_GNU_SOURCE || (_POSIX_C_SOURCE < 200112L && _XOPEN_SOURCE < 600))
    // Thread safe (GNU-specific)
    const char* str = strerror_r(errno, err_str, buff_size);
    return std::string(str);
#else
    // Thread safe (XSI-compliant)
    const int success = strerror_r(errno, err_str, buff_size);
    if (success != 0) {
        return {};
    }
    return std::string(err_str);
#endif
}
