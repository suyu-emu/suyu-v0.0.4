#pragma once

#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"

namespace Tegra {

class ConstBufferAccessor {
public:
    ConstBufferAccessor(Tegra::Engines::Maxwell3D& maxwell3d) : maxwell3d(maxwell3d) {}
    ~ConstBufferAccessor() = default;

    u32 access32(Tegra::Engines::Maxwell3D::Regs::ShaderStage stage, u64 const_buffer, u64 offset);

    u64 access64(Tegra::Engines::Maxwell3D::Regs::ShaderStage stage, u64 const_buffer, u64 offset);

private:
    Tegra::Engines::Maxwell3D& maxwell3d;
};

} // namespace Tegra
