// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/alignment.h"
#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/control/channel_state.h"
#include "video_core/engines/draw_manager.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/host1x/host1x.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_metal/mtl_command_recorder.h"
#include "video_core/renderer_metal/mtl_device.h"
#include "video_core/renderer_metal/mtl_rasterizer.h"
#include "video_core/texture_cache/texture_cache_base.h"

namespace Metal {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;
using MaxwellDrawState = Tegra::Engines::DrawManager::State;
using VideoCommon::ImageViewId;
using VideoCommon::ImageViewType;

namespace {

struct DrawParams {
    u32 base_instance;
    u32 num_instances;
    u32 base_vertex;
    u32 num_vertices;
    u32 first_index;
    bool is_indexed;
};

DrawParams MakeDrawParams(const MaxwellDrawState& draw_state, u32 num_instances, bool is_indexed) {
    DrawParams params{
        .base_instance = draw_state.base_instance,
        .num_instances = num_instances,
        .base_vertex = is_indexed ? draw_state.base_index : draw_state.vertex_buffer.first,
        .num_vertices = is_indexed ? draw_state.index_buffer.count : draw_state.vertex_buffer.count,
        .first_index = is_indexed ? draw_state.index_buffer.first : 0,
        .is_indexed = is_indexed,
    };

    // 6 triangle vertices per quad, base vertex is part of the index
    // See BindQuadIndexBuffer for more details
    if (draw_state.topology == Maxwell::PrimitiveTopology::Quads) {
        params.num_vertices = (params.num_vertices / 4) * 6;
        params.base_vertex = 0;
        params.is_indexed = true;
    } else if (draw_state.topology == Maxwell::PrimitiveTopology::QuadStrip) {
        params.num_vertices = (params.num_vertices - 2) / 2 * 6;
        params.base_vertex = 0;
        params.is_indexed = true;
    }

    return params;
}

} // Anonymous namespace

AccelerateDMA::AccelerateDMA(BufferCache& buffer_cache_) : buffer_cache{buffer_cache_} {}

bool AccelerateDMA::BufferCopy(GPUVAddr src_address, GPUVAddr dest_address, u64 amount) {
    return buffer_cache.DMACopy(src_address, dest_address, amount);
}
bool AccelerateDMA::BufferClear(GPUVAddr src_address, u64 amount, u32 value) {
    return buffer_cache.DMAClear(src_address, amount, value);
}

RasterizerMetal::RasterizerMetal(Tegra::GPU& gpu_,
                                 Tegra::MaxwellDeviceMemoryManager& device_memory_,
                                 const Device& device_, CommandRecorder& command_recorder_,
                                 const SwapChain& swap_chain_)
    : gpu{gpu_}, device_memory{device_memory_}, device{device_},
      command_recorder{command_recorder_}, swap_chain{swap_chain_},
      staging_buffer_pool(device, command_recorder),
      buffer_cache_runtime(device, command_recorder, staging_buffer_pool),
      buffer_cache(device_memory, buffer_cache_runtime),
      texture_cache_runtime(device, command_recorder, staging_buffer_pool),
      texture_cache(texture_cache_runtime, device_memory),
      pipeline_cache(device_memory, device, command_recorder, buffer_cache, texture_cache,
                     gpu.ShaderNotify()), accelerate_dma(buffer_cache) {}
RasterizerMetal::~RasterizerMetal() = default;

void RasterizerMetal::Draw(bool is_indexed, u32 instance_count) {
    // Bind the current graphics pipeline
    GraphicsPipeline* const pipeline{pipeline_cache.CurrentGraphicsPipeline()};
    if (!pipeline) {
        return;
    }

    // Set the engine
    pipeline->SetEngine(maxwell3d, gpu_memory);
    pipeline->Configure(is_indexed);

    const auto& draw_state = maxwell3d->draw_manager->GetDrawState();
    const DrawParams draw_params{MakeDrawParams(draw_state, instance_count, is_indexed)};

    // TODO: get the primitive type
    MTL::PrimitiveType primitiveType = MTL::PrimitiveTypeTriangle;//MaxwellToMTL::PrimitiveType(draw_state.topology);

    if (is_indexed) {
        auto& index_buffer = command_recorder.GetBoundIndexBuffer();
        size_t index_buffer_offset = index_buffer.offset + draw_params.first_index * index_buffer.index_size;

        ASSERT(index_buffer_offset % 4 == 0);

        command_recorder.GetRenderCommandEncoder()->drawIndexedPrimitives(primitiveType, draw_params.num_vertices, index_buffer.index_type, index_buffer.buffer, index_buffer_offset, draw_params.num_instances,
            draw_params.base_vertex, draw_params.base_instance);
    } else {
        command_recorder.GetRenderCommandEncoder()->drawPrimitives(primitiveType,
                                                                   draw_params.base_vertex, draw_params.num_vertices, draw_params.num_instances, draw_params.base_instance);
    }
}

void RasterizerMetal::DrawTexture() {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::Clear(u32 layer_count) {
    LOG_DEBUG(Render_Metal, "called");

    texture_cache.UpdateRenderTargets(true);
    const Framebuffer* const framebuffer = texture_cache.GetFramebuffer();
    if (!framebuffer) {
        return;
    }

    // TODO: clear
    command_recorder.BeginOrContinueRenderPass(framebuffer->GetHandle());
}

void RasterizerMetal::DispatchCompute() {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::ResetCounter(VideoCommon::QueryType type) {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::Query(GPUVAddr gpu_addr, VideoCommon::QueryType type,
                            VideoCommon::QueryPropertiesFlags flags, u32 payload, u32 subreport) {
    LOG_DEBUG(Render_Metal, "called");

    // TODO: remove this
    if (!gpu_memory) {
        return;
    }

    if (True(flags & VideoCommon::QueryPropertiesFlags::HasTimeout)) {
        u64 ticks = gpu.GetTicks();
        gpu_memory->Write<u64>(gpu_addr + 8, ticks);
        gpu_memory->Write<u64>(gpu_addr, static_cast<u64>(payload));
    } else {
        gpu_memory->Write<u32>(gpu_addr, payload);
    }
}

void RasterizerMetal::BindGraphicsUniformBuffer(size_t stage, u32 index, GPUVAddr gpu_addr,
                                                u32 size) {
    buffer_cache.BindGraphicsUniformBuffer(stage, index, gpu_addr, size);
}

void RasterizerMetal::DisableGraphicsUniformBuffer(size_t stage, u32 index) {
    buffer_cache.DisableGraphicsUniformBuffer(stage, index);
}

void RasterizerMetal::FlushAll() {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::FlushRegion(DAddr addr, u64 size, VideoCommon::CacheType which) {
    if (addr == 0 || size == 0) {
        return;
    }

    if (True(which & VideoCommon::CacheType::TextureCache)) {
        texture_cache.DownloadMemory(addr, size);
    }
    if ((True(which & VideoCommon::CacheType::BufferCache))) {
        buffer_cache.DownloadMemory(addr, size);
    }
    //if ((True(which & VideoCommon::CacheType::QueryCache))) {
    //    query_cache.FlushRegion(addr, size);
    //}
}

bool RasterizerMetal::MustFlushRegion(DAddr addr, u64 size, VideoCommon::CacheType which) {
    if ((True(which & VideoCommon::CacheType::BufferCache))) {
        if (buffer_cache.IsRegionGpuModified(addr, size)) {
            return true;
        }
    }
    if (!Settings::IsGPULevelHigh()) {
        return false;
    }
    if (True(which & VideoCommon::CacheType::TextureCache)) {
        return texture_cache.IsRegionGpuModified(addr, size);
    }
    return false;
}

void RasterizerMetal::InvalidateRegion(DAddr addr, u64 size, VideoCommon::CacheType which) {
    // TODO: inner invalidation
    /*
    for (const auto& [addr, size] : sequences) {
        texture_cache.WriteMemory(addr, size);
    }

    for (const auto& [addr, size] : sequences) {
        buffer_cache.WriteMemory(addr, size);
    }

    for (const auto& [addr, size] : sequences) {
        query_cache.InvalidateRegion(addr, size);
        pipeline_cache.InvalidateRegion(addr, size);
    }
    */

    if (addr == 0 || size == 0) {
        return;
    }

    if (True(which & VideoCommon::CacheType::TextureCache)) {
        texture_cache.WriteMemory(addr, size);
    }
    if ((True(which & VideoCommon::CacheType::BufferCache))) {
        buffer_cache.WriteMemory(addr, size);
    }
    //if ((True(which & VideoCommon::CacheType::QueryCache))) {
    //    query_cache.InvalidateRegion(addr, size);
    //}
    if ((True(which & VideoCommon::CacheType::ShaderCache))) {
        pipeline_cache.InvalidateRegion(addr, size);
    }
}

bool RasterizerMetal::OnCPUWrite(PAddr addr, u64 size) {
    if (addr == 0 || size == 0) {
        return false;
    }

    if (buffer_cache.OnCPUWrite(addr, size)) {
        return true;
    }
    texture_cache.WriteMemory(addr, size);

    pipeline_cache.InvalidateRegion(addr, size);

    return false;
}

void RasterizerMetal::OnCacheInvalidation(PAddr addr, u64 size) {
    if (addr == 0 || size == 0) {
        return;
    }

    texture_cache.WriteMemory(addr, size);
    buffer_cache.WriteMemory(addr, size);

    pipeline_cache.InvalidateRegion(addr, size);
}

VideoCore::RasterizerDownloadArea RasterizerMetal::GetFlushArea(PAddr addr, u64 size) {
    LOG_DEBUG(Render_Metal, "called");

    VideoCore::RasterizerDownloadArea new_area{
        .start_address = Common::AlignDown(addr, Core::DEVICE_PAGESIZE),
        .end_address = Common::AlignUp(addr + size, Core::DEVICE_PAGESIZE),
        .preemtive = true,
    };

    return new_area;
}

void RasterizerMetal::InvalidateGPUCache() {
    gpu.InvalidateGPUCache();
}

void RasterizerMetal::UnmapMemory(DAddr addr, u64 size) {
    texture_cache.UnmapMemory(addr, size);
    buffer_cache.WriteMemory(addr, size);

    pipeline_cache.OnCacheInvalidation(addr, size);
}

void RasterizerMetal::ModifyGPUMemory(size_t as_id, GPUVAddr addr, u64 size) {
    texture_cache.UnmapGPUMemory(as_id, addr, size);
}

void RasterizerMetal::SignalFence(std::function<void()>&& func) {
    LOG_DEBUG(Render_Metal, "called");

    func();
}

void RasterizerMetal::SyncOperation(std::function<void()>&& func) {
    LOG_DEBUG(Render_Metal, "called");

    func();
}

void RasterizerMetal::SignalSyncPoint(u32 value) {
    LOG_DEBUG(Render_Metal, "called");

    auto& syncpoint_manager = gpu.Host1x().GetSyncpointManager();
    syncpoint_manager.IncrementGuest(value);
    syncpoint_manager.IncrementHost(value);
}

void RasterizerMetal::SignalReference() {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::ReleaseFences(bool) {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::FlushAndInvalidateRegion(DAddr addr, u64 size, VideoCommon::CacheType which) {
    if (Settings::IsGPULevelExtreme()) {
        FlushRegion(addr, size, which);
    }
    InvalidateRegion(addr, size, which);
}

void RasterizerMetal::WaitForIdle() {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::FragmentBarrier() {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::TiledCacheBarrier() {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::FlushCommands() {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::TickFrame() {
    LOG_DEBUG(Render_Metal, "called");
}

Tegra::Engines::AccelerateDMAInterface& RasterizerMetal::AccessAccelerateDMA() {
    return accelerate_dma;
}

bool RasterizerMetal::AccelerateSurfaceCopy(const Tegra::Engines::Fermi2D::Surface& src,
                                            const Tegra::Engines::Fermi2D::Surface& dst,
                                            const Tegra::Engines::Fermi2D::Config& copy_config) {
    LOG_DEBUG(Render_Metal, "called");

    return true;
}

void RasterizerMetal::AccelerateInlineToMemory(GPUVAddr address, size_t copy_size,
                                               std::span<const u8> memory) {
    auto cpu_addr = gpu_memory->GpuToCpuAddress(address);
    if (!cpu_addr) [[unlikely]] {
        gpu_memory->WriteBlock(address, memory.data(), copy_size);
        return;
    }

    gpu_memory->WriteBlockUnsafe(address, memory.data(), copy_size);
    {
        if (!buffer_cache.InlineMemory(*cpu_addr, copy_size, memory)) {
            buffer_cache.WriteMemory(*cpu_addr, copy_size);
        }
    }

    texture_cache.WriteMemory(*cpu_addr, copy_size);

    pipeline_cache.InvalidateRegion(*cpu_addr, copy_size);
}

void RasterizerMetal::LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                                        const VideoCore::DiskResourceLoadCallback& callback) {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::InitializeChannel(Tegra::Control::ChannelState& channel) {
    CreateChannel(channel);
    buffer_cache.CreateChannel(channel);
    texture_cache.CreateChannel(channel);
    pipeline_cache.CreateChannel(channel);
}

void RasterizerMetal::BindChannel(Tegra::Control::ChannelState& channel) {
    BindToChannel(channel.bind_id);
    buffer_cache.BindToChannel(channel.bind_id);
    texture_cache.BindToChannel(channel.bind_id);
    pipeline_cache.BindToChannel(channel.bind_id);
}

void RasterizerMetal::ReleaseChannel(s32 channel_id) {
    EraseChannel(channel_id);
    buffer_cache.EraseChannel(channel_id);
    texture_cache.EraseChannel(channel_id);
    pipeline_cache.EraseChannel(channel_id);
}

} // namespace Metal
