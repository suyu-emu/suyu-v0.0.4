// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/result.h"

namespace Service::Glue {

constexpr ResultCode ERR_OUTPUT_TOO_SMALL{0x3C9D};
constexpr ResultCode ERR_PROCESS_ID_ZERO{0x3E9D};
constexpr ResultCode ERR_TITLE_ID_ZERO{0x3E9D};
constexpr ResultCode ERR_ALREADY_ISSUED{0x549D};
constexpr ResultCode ERR_NONEXISTENT{0xCC9D};

} // namespace Service::Glue
