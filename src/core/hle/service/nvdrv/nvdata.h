#pragma once

#include <array>
#include "common/common_types.h"

namespace Service::Nvidia {

struct Fence {
    s32 id;
    u32 value;
};

static_assert(sizeof(Fence) == 8, "Fence has wrong size");

struct MultiFence {
    u32 num_fences;
    std::array<Fence, 4> fences;
};

enum class NvResult : u32 {
    Success = 0,
    TryAgain = 11,
};

} // namespace Service::Nvidia
