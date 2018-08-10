// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <type_traits>
#include "common/bit_field.h"
#include "common/common_types.h"

namespace Tegra {

enum class SubmissionMode : u32 {
    IncreasingOld = 0,
    Increasing = 1,
    NonIncreasingOld = 2,
    NonIncreasing = 3,
    Inline = 4,
    IncreaseOnce = 5
};

union CommandHeader {
    u32 hex;

    BitField<0, 13, u32> method;
    BitField<13, 3, u32> subchannel;

    BitField<16, 13, u32> arg_count;
    BitField<16, 13, u32> inline_data;

    BitField<29, 3, SubmissionMode> mode;
};
static_assert(std::is_standard_layout_v<CommandHeader>, "CommandHeader is not standard layout");
static_assert(sizeof(CommandHeader) == sizeof(u32), "CommandHeader has incorrect size!");

} // namespace Tegra
