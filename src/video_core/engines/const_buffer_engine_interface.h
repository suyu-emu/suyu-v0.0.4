// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Tegra::Engines {

enum class ShaderType : u32 {
    Vertex = 0,
    TesselationControl = 1,
    TesselationEval = 2,
    Geometry = 3,
    Fragment = 4,
    Compute = 5,
};

class ConstBufferEngineInterface {
public:
    virtual ~ConstBufferEngineInterface() {}
    virtual u32 AccessConstBuffer32(ShaderType stage, u64 const_buffer, u64 offset) const = 0;
};

}
