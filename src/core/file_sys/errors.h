// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/result.h"

namespace FileSys {

constexpr ResultCode ERROR_PATH_NOT_FOUND{ErrorModule::FS, 1};
constexpr ResultCode ERROR_ENTITY_NOT_FOUND{ErrorModule::FS, 1002};
constexpr ResultCode ERROR_SD_CARD_NOT_FOUND{ErrorModule::FS, 2001};
constexpr ResultCode ERROR_INVALID_OFFSET{ErrorModule::FS, 6061};
constexpr ResultCode ERROR_INVALID_SIZE{ErrorModule::FS, 6062};

} // namespace FileSys
