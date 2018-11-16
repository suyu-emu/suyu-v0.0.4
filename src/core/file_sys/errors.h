// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/result.h"

namespace FileSys {

// TODO(bunnei): Replace these with correct errors for Switch OS
constexpr ResultCode ERROR_INVALID_PATH(-1);
constexpr ResultCode ERROR_UNSUPPORTED_OPEN_FLAGS(-1);
constexpr ResultCode ERROR_INVALID_OPEN_FLAGS(-1);
constexpr ResultCode ERROR_FILE_NOT_FOUND(-1);
constexpr ResultCode ERROR_UNEXPECTED_FILE_OR_DIRECTORY(-1);
constexpr ResultCode ERROR_DIRECTORY_ALREADY_EXISTS(-1);
constexpr ResultCode ERROR_FILE_ALREADY_EXISTS(-1);
constexpr ResultCode ERROR_DIRECTORY_NOT_EMPTY(-1);

constexpr ResultCode ERROR_PATH_NOT_FOUND{ErrorModule::FS, 1};
constexpr ResultCode ERROR_ENTITY_NOT_FOUND{ErrorModule::FS, 1002};
constexpr ResultCode ERROR_SD_CARD_NOT_FOUND{ErrorModule::FS, 2001};
constexpr ResultCode ERROR_INVALID_OFFSET{ErrorModule::FS, 6061};
constexpr ResultCode ERROR_INVALID_SIZE{ErrorModule::FS, 6062};

} // namespace FileSys
