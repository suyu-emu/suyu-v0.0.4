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
namespace {

// Match NVK/Nouveau's initial pushbuffer subchannel layout.
constexpr u32 Nvk3DSubchannel = 0;
constexpr u32 NvkComputeSubchannel = 1;
constexpr u32 Nvk2DSubchannel = 3;
constexpr u32 NvkCopySubchannel = 4;

void BindNvkDefaultSubchannels(ChannelState::Payload& payload) {
    auto& dma_pusher = payload.dma_pusher;
    dma_pusher.BindSubchannel(&payload.maxwell_3d, Nvk3DSubchannel, Engines::EngineTypes::Maxwell3D);
    dma_pusher.BindSubchannel(&payload.kepler_compute, NvkComputeSubchannel,
                              Engines::EngineTypes::KeplerCompute);
    // Subchannel 2 is M2MF there; Eden does not expose a 0x9039 engine yet.
    dma_pusher.BindSubchannel(&payload.fermi_2d, Nvk2DSubchannel, Engines::EngineTypes::Fermi2D);
    dma_pusher.BindSubchannel(&payload.maxwell_dma, NvkCopySubchannel,
                              Engines::EngineTypes::MaxwellDMA);
}

} // Anonymous namespace

ChannelState::Payload::Payload(Core::System& system, MemoryManager& memory_manager, ChannelState& channel_state)
    : maxwell_3d(memory_manager)
    , fermi_2d(memory_manager)
    , kepler_compute(memory_manager)
    , maxwell_dma(memory_manager)
    , kepler_memory(memory_manager)
    , nv01_timer(memory_manager)
    , dma_pusher(system, memory_manager, channel_state)
{}

ChannelState::ChannelState(s32 bind_id_)
    : bind_id{bind_id_}
{}

void ChannelState::Init(Core::System& system, u64 program_id_) {
    ASSERT(memory_manager);
    program_id = program_id_;
    payload.emplace(system, *memory_manager, *this);
    BindNvkDefaultSubchannels(*payload);
    initialized = true;
}

void ChannelState::BindRasterizer(VideoCore::RasterizerInterface* rasterizer) {
    payload->dma_pusher.BindRasterizer(rasterizer);
    memory_manager->BindRasterizer(rasterizer);
    payload->maxwell_3d.BindRasterizer(rasterizer);
    payload->fermi_2d.BindRasterizer(rasterizer);
    payload->kepler_memory.BindRasterizer(rasterizer);
    payload->kepler_compute.BindRasterizer(rasterizer);
    payload->maxwell_dma.BindRasterizer(rasterizer);
    //payload->nv01_timer.BindRasterizer(rasterizer);
}

} // namespace Tegra::Control
