// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace Common {
struct WebResult {
    enum class Code : u32 {
        Success,
        InvalidURL,
        CredentialsMissing,
        LibError,
        HttpError,
        WrongContent,
        NoWebservice,
    };
    Code result_code;
    std::string result_string;
    std::string returned_data;
};
} // namespace Common
