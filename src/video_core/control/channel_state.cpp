// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/assert.h"
#include "video_core/control/channel_state.h"
#include "video_core/dma_pusher.h"
#include "video_core/engines/fermi_2d.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/kepler_memory.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/maxwell_dma.h"
#include "video_core/engines/puller.h"
#include "video_core/memory_manager.h"

namespace Tegra::Control {

ChannelState::ChannelState(s32 bind_id_) : bind_id{bind_id_}, initialized{} {}

void ChannelState::Init(Core::System& system, GPU& gpu, u64 program_id_) {
    ASSERT(memory_manager);
    program_id = program_id_;
    dma_pusher.emplace(system, gpu, *memory_manager, *this);
    maxwell_3d.emplace(system, *memory_manager);
    fermi_2d.emplace(*memory_manager);
    kepler_compute.emplace(system, *memory_manager);
    maxwell_dma.emplace(system, *memory_manager);
    kepler_memory.emplace(system, *memory_manager);
    initialized = true;
}

void ChannelState::BindRasterizer(VideoCore::RasterizerInterface* rasterizer) {
    dma_pusher->BindRasterizer(rasterizer);
    memory_manager->BindRasterizer(rasterizer);
    maxwell_3d->BindRasterizer(rasterizer);
    fermi_2d->BindRasterizer(rasterizer);
    kepler_memory->BindRasterizer(rasterizer);
    kepler_compute->BindRasterizer(rasterizer);
    maxwell_dma->BindRasterizer(rasterizer);
}

} // namespace Tegra::Control
