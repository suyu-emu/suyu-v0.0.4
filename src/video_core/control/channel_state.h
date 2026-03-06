// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include "common/common_types.h"
#include "video_core/engines/fermi_2d.h"
#include "video_core/engines/kepler_memory.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/maxwell_dma.h"
#include "video_core/dma_pusher.h"

namespace Core {
class System;
}

namespace VideoCore {
class RasterizerInterface;
}

namespace Tegra {

class GPU;
class MemoryManager;

namespace Control {

struct ChannelState {
    explicit ChannelState(s32 bind_id);

    void Init(Core::System& system, GPU& gpu, u64 program_id);

    void BindRasterizer(VideoCore::RasterizerInterface* rasterizer);

    /// 3D engine
    std::optional<Engines::Maxwell3D> maxwell_3d;
    /// 2D engine
    std::optional<Engines::Fermi2D> fermi_2d;
    /// Compute engine
    std::optional<Engines::KeplerCompute> kepler_compute;
    /// DMA engine
    std::optional<Engines::MaxwellDMA> maxwell_dma;
    /// Inline memory engine
    std::optional<Engines::KeplerMemory> kepler_memory;
    /// NV01 Timer
    std::optional<Engines::KeplerMemory> nv01_timer;
    std::optional<DmaPusher> dma_pusher;
    std::shared_ptr<MemoryManager> memory_manager;

    s32 bind_id = -1;
    u64 program_id = 0;
    bool initialized{};
};

} // namespace Control

} // namespace Tegra
