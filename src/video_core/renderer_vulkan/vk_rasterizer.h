// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <bitset>
#include <memory>
#include <utility>
#include <vector>

#include <boost/container/static_vector.hpp>
#include <boost/functional/hash.hpp>

#include "common/common_types.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_accelerated.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_compute_pass.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_fence_manager.h"
#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_query_cache.h"
#include "video_core/renderer_vulkan/vk_renderpass_cache.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_sampler_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Core {
class System;
}

namespace Core::Frontend {
class EmuWindow;
}

namespace Tegra::Engines {
class Maxwell3D;
}

namespace Vulkan {

struct VKScreenInfo;

using ImageViewsPack = boost::container::static_vector<VkImageView, Maxwell::NumRenderTargets + 1>;

struct FramebufferCacheKey {
    VkRenderPass renderpass{};
    u32 width = 0;
    u32 height = 0;
    u32 layers = 0;
    ImageViewsPack views;

    std::size_t Hash() const noexcept {
        std::size_t hash = 0;
        boost::hash_combine(hash, static_cast<VkRenderPass>(renderpass));
        for (const auto& view : views) {
            boost::hash_combine(hash, static_cast<VkImageView>(view));
        }
        boost::hash_combine(hash, width);
        boost::hash_combine(hash, height);
        boost::hash_combine(hash, layers);
        return hash;
    }

    bool operator==(const FramebufferCacheKey& rhs) const noexcept {
        return std::tie(renderpass, views, width, height, layers) ==
               std::tie(rhs.renderpass, rhs.views, rhs.width, rhs.height, rhs.layers);
    }

    bool operator!=(const FramebufferCacheKey& rhs) const noexcept {
        return !operator==(rhs);
    }
};

} // namespace Vulkan

namespace std {

template <>
struct hash<Vulkan::FramebufferCacheKey> {
    std::size_t operator()(const Vulkan::FramebufferCacheKey& k) const noexcept {
        return k.Hash();
    }
};

} // namespace std

namespace Vulkan {

class StateTracker;
class BufferBindings;

struct ImageView {
    View view;
    VkImageLayout* layout = nullptr;
};

class RasterizerVulkan final : public VideoCore::RasterizerAccelerated {
public:
    explicit RasterizerVulkan(Core::System& system, Core::Frontend::EmuWindow& render_window,
                              VKScreenInfo& screen_info, const VKDevice& device,
                              VKResourceManager& resource_manager, VKMemoryManager& memory_manager,
                              StateTracker& state_tracker, VKScheduler& scheduler);
    ~RasterizerVulkan() override;

    void Draw(bool is_indexed, bool is_instanced) override;
    void Clear() override;
    void DispatchCompute(GPUVAddr code_addr) override;
    void ResetCounter(VideoCore::QueryType type) override;
    void Query(GPUVAddr gpu_addr, VideoCore::QueryType type, std::optional<u64> timestamp) override;
    void FlushAll() override;
    void FlushRegion(VAddr addr, u64 size) override;
    bool MustFlushRegion(VAddr addr, u64 size) override;
    void InvalidateRegion(VAddr addr, u64 size) override;
    void OnCPUWrite(VAddr addr, u64 size) override;
    void SyncGuestHost() override;
    void SignalSemaphore(GPUVAddr addr, u32 value) override;
    void SignalSyncPoint(u32 value) override;
    void ReleaseFences() override;
    void FlushAndInvalidateRegion(VAddr addr, u64 size) override;
    void FlushCommands() override;
    void TickFrame() override;
    bool AccelerateSurfaceCopy(const Tegra::Engines::Fermi2D::Regs::Surface& src,
                               const Tegra::Engines::Fermi2D::Regs::Surface& dst,
                               const Tegra::Engines::Fermi2D::Config& copy_config) override;
    bool AccelerateDisplay(const Tegra::FramebufferConfig& config, VAddr framebuffer_addr,
                           u32 pixel_stride) override;
    void SetupDirtyFlags() override;

    /// Maximum supported size that a constbuffer can have in bytes.
    static constexpr std::size_t MaxConstbufferSize = 0x10000;
    static_assert(MaxConstbufferSize % (4 * sizeof(float)) == 0,
                  "The maximum size of a constbuffer must be a multiple of the size of GLvec4");

private:
    struct DrawParameters {
        void Draw(vk::CommandBuffer cmdbuf) const;

        u32 base_instance = 0;
        u32 num_instances = 0;
        u32 base_vertex = 0;
        u32 num_vertices = 0;
        bool is_indexed = 0;
    };

    using Texceptions = std::bitset<Maxwell::NumRenderTargets + 1>;

    static constexpr std::size_t ZETA_TEXCEPTION_INDEX = 8;

    void FlushWork();

    Texceptions UpdateAttachments();

    std::tuple<VkFramebuffer, VkExtent2D> ConfigureFramebuffers(VkRenderPass renderpass);

    /// Setups geometry buffers and state.
    DrawParameters SetupGeometry(FixedPipelineState& fixed_state, BufferBindings& buffer_bindings,
                                 bool is_indexed, bool is_instanced);

    /// Setup descriptors in the graphics pipeline.
    void SetupShaderDescriptors(const std::array<Shader, Maxwell::MaxShaderProgram>& shaders);

    void SetupImageTransitions(Texceptions texceptions,
                               const std::array<View, Maxwell::NumRenderTargets>& color_attachments,
                               const View& zeta_attachment);

    void UpdateDynamicStates();

    void BeginTransformFeedback();

    void EndTransformFeedback();

    bool WalkAttachmentOverlaps(const CachedSurfaceView& attachment);

    void SetupVertexArrays(FixedPipelineState::VertexInput& vertex_input,
                           BufferBindings& buffer_bindings);

    void SetupIndexBuffer(BufferBindings& buffer_bindings, DrawParameters& params, bool is_indexed);

    /// Setup constant buffers in the graphics pipeline.
    void SetupGraphicsConstBuffers(const ShaderEntries& entries, std::size_t stage);

    /// Setup global buffers in the graphics pipeline.
    void SetupGraphicsGlobalBuffers(const ShaderEntries& entries, std::size_t stage);

    /// Setup texel buffers in the graphics pipeline.
    void SetupGraphicsTexelBuffers(const ShaderEntries& entries, std::size_t stage);

    /// Setup textures in the graphics pipeline.
    void SetupGraphicsTextures(const ShaderEntries& entries, std::size_t stage);

    /// Setup images in the graphics pipeline.
    void SetupGraphicsImages(const ShaderEntries& entries, std::size_t stage);

    /// Setup constant buffers in the compute pipeline.
    void SetupComputeConstBuffers(const ShaderEntries& entries);

    /// Setup global buffers in the compute pipeline.
    void SetupComputeGlobalBuffers(const ShaderEntries& entries);

    /// Setup texel buffers in the compute pipeline.
    void SetupComputeTexelBuffers(const ShaderEntries& entries);

    /// Setup textures in the compute pipeline.
    void SetupComputeTextures(const ShaderEntries& entries);

    /// Setup images in the compute pipeline.
    void SetupComputeImages(const ShaderEntries& entries);

    void SetupConstBuffer(const ConstBufferEntry& entry,
                          const Tegra::Engines::ConstBufferInfo& buffer);

    void SetupGlobalBuffer(const GlobalBufferEntry& entry, GPUVAddr address);

    void SetupTexelBuffer(const Tegra::Texture::TICEntry& image, const TexelBufferEntry& entry);

    void SetupTexture(const Tegra::Texture::FullTextureInfo& texture, const SamplerEntry& entry);

    void SetupImage(const Tegra::Texture::TICEntry& tic, const ImageEntry& entry);

    void UpdateViewportsState(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateScissorsState(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateDepthBias(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateBlendConstants(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateDepthBounds(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateStencilFaces(Tegra::Engines::Maxwell3D::Regs& regs);

    std::size_t CalculateGraphicsStreamBufferSize(bool is_indexed) const;

    std::size_t CalculateComputeStreamBufferSize() const;

    std::size_t CalculateVertexArraysSize() const;

    std::size_t CalculateIndexBufferSize() const;

    std::size_t CalculateConstBufferSize(const ConstBufferEntry& entry,
                                         const Tegra::Engines::ConstBufferInfo& buffer) const;

    RenderPassParams GetRenderPassParams(Texceptions texceptions) const;

    Core::System& system;
    Core::Frontend::EmuWindow& render_window;
    VKScreenInfo& screen_info;
    const VKDevice& device;
    VKResourceManager& resource_manager;
    VKMemoryManager& memory_manager;
    StateTracker& state_tracker;
    VKScheduler& scheduler;

    VKStagingBufferPool staging_pool;
    VKDescriptorPool descriptor_pool;
    VKUpdateDescriptorQueue update_descriptor_queue;
    VKRenderPassCache renderpass_cache;
    QuadArrayPass quad_array_pass;
    QuadIndexedPass quad_indexed_pass;
    Uint8Pass uint8_pass;

    VKTextureCache texture_cache;
    VKPipelineCache pipeline_cache;
    VKBufferCache buffer_cache;
    VKSamplerCache sampler_cache;
    VKFenceManager fence_manager;
    VKQueryCache query_cache;

    std::array<View, Maxwell::NumRenderTargets> color_attachments;
    View zeta_attachment;

    std::vector<ImageView> sampled_views;
    std::vector<ImageView> image_views;

    u32 draw_counter = 0;

    // TODO(Rodrigo): Invalidate on image destruction
    std::unordered_map<FramebufferCacheKey, vk::Framebuffer> framebuffer_cache;
};

} // namespace Vulkan
