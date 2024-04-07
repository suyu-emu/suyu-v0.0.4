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

AccelerateDMA::AccelerateDMA() = default;

bool AccelerateDMA::BufferCopy(GPUVAddr start_address, GPUVAddr end_address, u64 amount) {
    return true;
}
bool AccelerateDMA::BufferClear(GPUVAddr src_address, u64 amount, u32 value) {
    return true;
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
      texture_cache(texture_cache_runtime, device_memory) {}
RasterizerMetal::~RasterizerMetal() = default;

void RasterizerMetal::Draw(bool is_indexed, u32 instance_count) {
    // TODO: uncomment
    // command_recorder.CheckIfRenderPassIsActive();
    // const auto& draw_state = maxwell3d->draw_manager->GetDrawState();
    if (is_indexed) {
        LOG_DEBUG(Render_Metal, "indexed");
        /*[command_buffer drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                   indexCount:draw_params.num_indices
                                    indexType:MTLIndexTypeUInt32
                                  indexBuffer:draw_state.index_buffer
                            indexBufferOffset:draw_params.first_index * sizeof(u32)
                                instanceCount:draw_params.num_instances
                                   baseVertex:draw_params.base_vertex
                                 baseInstance:draw_params.base_instance];*/
        // cmdbuf.DrawIndexed(draw_params.num_vertices, draw_params.num_instances,
        //                     draw_params.first_index, draw_params.base_vertex,
        //                     draw_params.base_instance);
    } else {
        LOG_DEBUG(Render_Metal, "not indexed");
        // cmdbuf.Draw(draw_params.num_vertices, draw_params.num_instances,
        //             draw_params.base_vertex, draw_params.base_instance);
    }

    // HACK: test is buffers are being correctly created
    buffer_cache.UpdateGraphicsBuffers(is_indexed);
    buffer_cache.BindHostGeometryBuffers(is_indexed);
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

    // Begin render pass
    command_recorder.BeginRenderPass(framebuffer->GetHandle());
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
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::DisableGraphicsUniformBuffer(size_t stage, u32 index) {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::FlushAll() {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::FlushRegion(DAddr addr, u64 size, VideoCommon::CacheType) {
    LOG_DEBUG(Render_Metal, "called");
}

bool RasterizerMetal::MustFlushRegion(DAddr addr, u64 size, VideoCommon::CacheType) {
    return false;
}

void RasterizerMetal::InvalidateRegion(DAddr addr, u64 size, VideoCommon::CacheType) {
    LOG_DEBUG(Render_Metal, "called");
}

bool RasterizerMetal::OnCPUWrite(PAddr addr, u64 size) {
    return false;
}

void RasterizerMetal::OnCacheInvalidation(PAddr addr, u64 size) {
    LOG_DEBUG(Render_Metal, "called");
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
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::UnmapMemory(DAddr addr, u64 size) {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::ModifyGPUMemory(size_t as_id, GPUVAddr addr, u64 size) {
    LOG_DEBUG(Render_Metal, "called");
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

void RasterizerMetal::FlushAndInvalidateRegion(DAddr addr, u64 size, VideoCommon::CacheType) {
    LOG_DEBUG(Render_Metal, "called");
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
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                                        const VideoCore::DiskResourceLoadCallback& callback) {
    LOG_DEBUG(Render_Metal, "called");
}

void RasterizerMetal::InitializeChannel(Tegra::Control::ChannelState& channel) {
    LOG_DEBUG(Render_Metal, "called");

    CreateChannel(channel);
    buffer_cache.CreateChannel(channel);
    texture_cache.CreateChannel(channel);
}

void RasterizerMetal::BindChannel(Tegra::Control::ChannelState& channel) {
    LOG_DEBUG(Render_Metal, "called");

    BindToChannel(channel.bind_id);
    buffer_cache.BindToChannel(channel.bind_id);
    texture_cache.BindToChannel(channel.bind_id);
}

void RasterizerMetal::ReleaseChannel(s32 channel_id) {
    LOG_DEBUG(Render_Metal, "called");

    EraseChannel(channel_id);
    buffer_cache.EraseChannel(channel_id);
    texture_cache.EraseChannel(channel_id);
}

} // namespace Metal
