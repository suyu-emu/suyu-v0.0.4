// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/result.h"

namespace FileSys {

namespace ErrCodes {
enum {
    NotFound = 1,
    EntityNotFound = 1002,
    SdCardNotFound = 2001,
    RomFSNotFound = 2520,
    InvalidOffset = 6061,
    InvalidSize = 6062,
};
}

constexpr ResultCode ERROR_PATH_NOT_FOUND(ErrorModule::FS, ErrCodes::NotFound);

// TODO(bunnei): Replace these with correct errors for Switch OS
constexpr ResultCode ERROR_INVALID_PATH(-1);
constexpr ResultCode ERROR_UNSUPPORTED_OPEN_FLAGS(-1);
constexpr ResultCode ERROR_INVALID_OPEN_FLAGS(-1);
constexpr ResultCode ERROR_FILE_NOT_FOUND(-1);
constexpr ResultCode ERROR_UNEXPECTED_FILE_OR_DIRECTORY(-1);
constexpr ResultCode ERROR_DIRECTORY_ALREADY_EXISTS(-1);
constexpr ResultCode ERROR_FILE_ALREADY_EXISTS(-1);
constexpr ResultCode ERROR_DIRECTORY_NOT_EMPTY(-1);

constexpr ResultCode ERROR_ENTITY_NOT_FOUND{ErrorModule::FS, ErrCodes::EntityNotFound};
constexpr ResultCode ERROR_SD_CARD_NOT_FOUND{ErrorModule::FS, ErrCodes::SdCardNotFound};
constexpr ResultCode ERROR_INVALID_OFFSET{ErrorModule::FS, ErrCodes::InvalidOffset};
constexpr ResultCode ERROR_INVALID_SIZE{ErrorModule::FS, ErrCodes::InvalidSize};

} // namespace FileSys
