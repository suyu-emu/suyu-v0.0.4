// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include "common/common_types.h"
#include "core/hle/service/nvdrv/nvdata.h"
#include "core/hle/service/nvflinger/buffer_queue.h"
#include "video_core/cdma_pusher.h"
#include "video_core/dma_pusher.h"

using CacheAddr = std::uintptr_t;
[[nodiscard]] inline CacheAddr ToCacheAddr(const void* host_ptr) {
    return reinterpret_cast<CacheAddr>(host_ptr);
}

[[nodiscard]] inline u8* FromCacheAddr(CacheAddr cache_addr) {
    return reinterpret_cast<u8*>(cache_addr);
}

namespace Core {
namespace Frontend {
class EmuWindow;
}
class System;
} // namespace Core

namespace VideoCore {
class RendererBase;
class ShaderNotify;
} // namespace VideoCore

namespace Tegra {

enum class RenderTargetFormat : u32 {
    NONE = 0x0,
    R32B32G32A32_FLOAT = 0xC0,
    R32G32B32A32_SINT = 0xC1,
    R32G32B32A32_UINT = 0xC2,
    R16G16B16A16_UNORM = 0xC6,
    R16G16B16A16_SNORM = 0xC7,
    R16G16B16A16_SINT = 0xC8,
    R16G16B16A16_UINT = 0xC9,
    R16G16B16A16_FLOAT = 0xCA,
    R32G32_FLOAT = 0xCB,
    R32G32_SINT = 0xCC,
    R32G32_UINT = 0xCD,
    R16G16B16X16_FLOAT = 0xCE,
    B8G8R8A8_UNORM = 0xCF,
    B8G8R8A8_SRGB = 0xD0,
    A2B10G10R10_UNORM = 0xD1,
    A2B10G10R10_UINT = 0xD2,
    A8B8G8R8_UNORM = 0xD5,
    A8B8G8R8_SRGB = 0xD6,
    A8B8G8R8_SNORM = 0xD7,
    A8B8G8R8_SINT = 0xD8,
    A8B8G8R8_UINT = 0xD9,
    R16G16_UNORM = 0xDA,
    R16G16_SNORM = 0xDB,
    R16G16_SINT = 0xDC,
    R16G16_UINT = 0xDD,
    R16G16_FLOAT = 0xDE,
    B10G11R11_FLOAT = 0xE0,
    R32_SINT = 0xE3,
    R32_UINT = 0xE4,
    R32_FLOAT = 0xE5,
    R5G6B5_UNORM = 0xE8,
    A1R5G5B5_UNORM = 0xE9,
    R8G8_UNORM = 0xEA,
    R8G8_SNORM = 0xEB,
    R8G8_SINT = 0xEC,
    R8G8_UINT = 0xED,
    R16_UNORM = 0xEE,
    R16_SNORM = 0xEF,
    R16_SINT = 0xF0,
    R16_UINT = 0xF1,
    R16_FLOAT = 0xF2,
    R8_UNORM = 0xF3,
    R8_SNORM = 0xF4,
    R8_SINT = 0xF5,
    R8_UINT = 0xF6,
};

enum class DepthFormat : u32 {
    D32_FLOAT = 0xA,
    D16_UNORM = 0x13,
    S8_UINT_Z24_UNORM = 0x14,
    D24X8_UNORM = 0x15,
    D24S8_UNORM = 0x16,
    D24C8_UNORM = 0x18,
    D32_FLOAT_S8X24_UINT = 0x19,
};

struct CommandListHeader;
class DebugContext;

/**
 * Struct describing framebuffer configuration
 */
struct FramebufferConfig {
    enum class PixelFormat : u32 {
        A8B8G8R8_UNORM = 1,
        RGB565_UNORM = 4,
        B8G8R8A8_UNORM = 5,
    };

    VAddr address;
    u32 offset;
    u32 width;
    u32 height;
    u32 stride;
    PixelFormat pixel_format;

    using TransformFlags = Service::NVFlinger::BufferQueue::BufferTransformFlags;
    TransformFlags transform_flags;
    Common::Rectangle<int> crop_rect;
};

namespace Engines {
class Fermi2D;
class Maxwell3D;
class MaxwellDMA;
class KeplerCompute;
class KeplerMemory;
} // namespace Engines

enum class EngineID {
    FERMI_TWOD_A = 0x902D, // 2D Engine
    MAXWELL_B = 0xB197,    // 3D Engine
    KEPLER_COMPUTE_B = 0xB1C0,
    KEPLER_INLINE_TO_MEMORY_B = 0xA140,
    MAXWELL_DMA_COPY_A = 0xB0B5,
};

class MemoryManager;

class GPU {
public:
    struct MethodCall {
        u32 method{};
        u32 argument{};
        u32 subchannel{};
        u32 method_count{};

        MethodCall(u32 method, u32 argument, u32 subchannel = 0, u32 method_count = 0)
            : method(method), argument(argument), subchannel(subchannel),
              method_count(method_count) {}

        [[nodiscard]] bool IsLastCall() const {
            return method_count <= 1;
        }
    };

    explicit GPU(Core::System& system, bool is_async, bool use_nvdec);
    virtual ~GPU();

    /// Binds a renderer to the GPU.
    void BindRenderer(std::unique_ptr<VideoCore::RendererBase> renderer);

    /// Calls a GPU method.
    void CallMethod(const MethodCall& method_call);

    /// Calls a GPU multivalue method.
    void CallMultiMethod(u32 method, u32 subchannel, const u32* base_start, u32 amount,
                         u32 methods_pending);

    /// Flush all current written commands into the host GPU for execution.
    void FlushCommands();
    /// Synchronizes CPU writes with Host GPU memory.
    void SyncGuestHost();
    /// Signal the ending of command list.
    virtual void OnCommandListEnd();

    /// Request a host GPU memory flush from the CPU.
    [[nodiscard]] u64 RequestFlush(VAddr addr, std::size_t size);

    /// Obtains current flush request fence id.
    [[nodiscard]] u64 CurrentFlushRequestFence() const {
        return current_flush_fence.load(std::memory_order_relaxed);
    }

    /// Tick pending requests within the GPU.
    void TickWork();

    /// Returns a reference to the Maxwell3D GPU engine.
    [[nodiscard]] Engines::Maxwell3D& Maxwell3D();

    /// Returns a const reference to the Maxwell3D GPU engine.
    [[nodiscard]] const Engines::Maxwell3D& Maxwell3D() const;

    /// Returns a reference to the KeplerCompute GPU engine.
    [[nodiscard]] Engines::KeplerCompute& KeplerCompute();

    /// Returns a reference to the KeplerCompute GPU engine.
    [[nodiscard]] const Engines::KeplerCompute& KeplerCompute() const;

    /// Returns a reference to the GPU memory manager.
    [[nodiscard]] Tegra::MemoryManager& MemoryManager();

    /// Returns a const reference to the GPU memory manager.
    [[nodiscard]] const Tegra::MemoryManager& MemoryManager() const;

    /// Returns a reference to the GPU DMA pusher.
    [[nodiscard]] Tegra::DmaPusher& DmaPusher();

    /// Returns a const reference to the GPU DMA pusher.
    [[nodiscard]] const Tegra::DmaPusher& DmaPusher() const;

    /// Returns a reference to the GPU CDMA pusher.
    [[nodiscard]] Tegra::CDmaPusher& CDmaPusher();

    /// Returns a const reference to the GPU CDMA pusher.
    [[nodiscard]] const Tegra::CDmaPusher& CDmaPusher() const;

    /// Returns a reference to the underlying renderer.
    [[nodiscard]] VideoCore::RendererBase& Renderer() {
        return *renderer;
    }

    /// Returns a const reference to the underlying renderer.
    [[nodiscard]] const VideoCore::RendererBase& Renderer() const {
        return *renderer;
    }

    /// Returns a reference to the shader notifier.
    [[nodiscard]] VideoCore::ShaderNotify& ShaderNotify() {
        return *shader_notify;
    }

    /// Returns a const reference to the shader notifier.
    [[nodiscard]] const VideoCore::ShaderNotify& ShaderNotify() const {
        return *shader_notify;
    }

    // Waits for the GPU to finish working
    virtual void WaitIdle() const = 0;

    /// Allows the CPU/NvFlinger to wait on the GPU before presenting a frame.
    void WaitFence(u32 syncpoint_id, u32 value);

    void IncrementSyncPoint(u32 syncpoint_id);

    [[nodiscard]] u32 GetSyncpointValue(u32 syncpoint_id) const;

    void RegisterSyncptInterrupt(u32 syncpoint_id, u32 value);

    [[nodiscard]] bool CancelSyncptInterrupt(u32 syncpoint_id, u32 value);

    [[nodiscard]] u64 GetTicks() const;

    [[nodiscard]] std::unique_lock<std::mutex> LockSync() {
        return std::unique_lock{sync_mutex};
    }

    [[nodiscard]] bool IsAsync() const {
        return is_async;
    }

    [[nodiscard]] bool UseNvdec() const {
        return use_nvdec;
    }

    enum class FenceOperation : u32 {
        Acquire = 0,
        Increment = 1,
    };

    union FenceAction {
        u32 raw;
        BitField<0, 1, FenceOperation> op;
        BitField<8, 24, u32> syncpoint_id;

        [[nodiscard]] static CommandHeader Build(FenceOperation op, u32 syncpoint_id) {
            FenceAction result{};
            result.op.Assign(op);
            result.syncpoint_id.Assign(syncpoint_id);
            return {result.raw};
        }
    };

    struct Regs {
        static constexpr size_t NUM_REGS = 0x40;

        union {
            struct {
                INSERT_UNION_PADDING_WORDS(0x4);
                struct {
                    u32 address_high;
                    u32 address_low;

                    [[nodiscard]] GPUVAddr SemaphoreAddress() const {
                        return static_cast<GPUVAddr>((static_cast<GPUVAddr>(address_high) << 32) |
                                                     address_low);
                    }
                } semaphore_address;

                u32 semaphore_sequence;
                u32 semaphore_trigger;
                INSERT_UNION_PADDING_WORDS(0xC);

                // The pusher and the puller share the reference counter, the pusher only has read
                // access
                u32 reference_count;
                INSERT_UNION_PADDING_WORDS(0x5);

                u32 semaphore_acquire;
                u32 semaphore_release;
                u32 fence_value;
                FenceAction fence_action;
                INSERT_UNION_PADDING_WORDS(0xE2);

                // Puller state
                u32 acquire_mode;
                u32 acquire_source;
                u32 acquire_active;
                u32 acquire_timeout;
                u32 acquire_value;
            };
            std::array<u32, NUM_REGS> reg_array;
        };
    } regs{};

    /// Performs any additional setup necessary in order to begin GPU emulation.
    /// This can be used to launch any necessary threads and register any necessary
    /// core timing events.
    virtual void Start() = 0;

    /// Obtain the CPU Context
    virtual void ObtainContext() = 0;

    /// Release the CPU Context
    virtual void ReleaseContext() = 0;

    /// Push GPU command entries to be processed
    virtual void PushGPUEntries(Tegra::CommandList&& entries) = 0;

    /// Push GPU command buffer entries to be processed
    virtual void PushCommandBuffer(Tegra::ChCommandHeaderList& entries) = 0;

    /// Swap buffers (render frame)
    virtual void SwapBuffers(const Tegra::FramebufferConfig* framebuffer) = 0;

    /// Notify rasterizer that any caches of the specified region should be flushed to Switch memory
    virtual void FlushRegion(VAddr addr, u64 size) = 0;

    /// Notify rasterizer that any caches of the specified region should be invalidated
    virtual void InvalidateRegion(VAddr addr, u64 size) = 0;

    /// Notify rasterizer that any caches of the specified region should be flushed and invalidated
    virtual void FlushAndInvalidateRegion(VAddr addr, u64 size) = 0;

protected:
    virtual void TriggerCpuInterrupt(u32 syncpoint_id, u32 value) const = 0;

private:
    void ProcessBindMethod(const MethodCall& method_call);
    void ProcessFenceActionMethod();
    void ProcessWaitForInterruptMethod();
    void ProcessSemaphoreTriggerMethod();
    void ProcessSemaphoreRelease();
    void ProcessSemaphoreAcquire();

    /// Calls a GPU puller method.
    void CallPullerMethod(const MethodCall& method_call);

    /// Calls a GPU engine method.
    void CallEngineMethod(const MethodCall& method_call);

    /// Calls a GPU engine multivalue method.
    void CallEngineMultiMethod(u32 method, u32 subchannel, const u32* base_start, u32 amount,
                               u32 methods_pending);

    /// Determines where the method should be executed.
    [[nodiscard]] bool ExecuteMethodOnEngine(u32 method);

protected:
    Core::System& system;
    std::unique_ptr<Tegra::MemoryManager> memory_manager;
    std::unique_ptr<Tegra::DmaPusher> dma_pusher;
    std::unique_ptr<Tegra::CDmaPusher> cdma_pusher;
    std::unique_ptr<VideoCore::RendererBase> renderer;
    const bool use_nvdec;

private:
    /// Mapping of command subchannels to their bound engine ids
    std::array<EngineID, 8> bound_engines = {};
    /// 3D engine
    std::unique_ptr<Engines::Maxwell3D> maxwell_3d;
    /// 2D engine
    std::unique_ptr<Engines::Fermi2D> fermi_2d;
    /// Compute engine
    std::unique_ptr<Engines::KeplerCompute> kepler_compute;
    /// DMA engine
    std::unique_ptr<Engines::MaxwellDMA> maxwell_dma;
    /// Inline memory engine
    std::unique_ptr<Engines::KeplerMemory> kepler_memory;
    /// Shader build notifier
    std::unique_ptr<VideoCore::ShaderNotify> shader_notify;

    std::array<std::atomic<u32>, Service::Nvidia::MaxSyncPoints> syncpoints{};

    std::array<std::list<u32>, Service::Nvidia::MaxSyncPoints> syncpt_interrupts;

    std::mutex sync_mutex;
    std::mutex device_mutex;

    std::condition_variable sync_cv;

    struct FlushRequest {
        FlushRequest(u64 fence, VAddr addr, std::size_t size)
            : fence{fence}, addr{addr}, size{size} {}
        u64 fence;
        VAddr addr;
        std::size_t size;
    };

    std::list<FlushRequest> flush_requests;
    std::atomic<u64> current_flush_fence{};
    u64 last_flush_fence{};
    std::mutex flush_request_mutex;

    const bool is_async;
};

#define ASSERT_REG_POSITION(field_name, position)                                                  \
    static_assert(offsetof(GPU::Regs, field_name) == position * 4,                                 \
                  "Field " #field_name " has invalid position")

ASSERT_REG_POSITION(semaphore_address, 0x4);
ASSERT_REG_POSITION(semaphore_sequence, 0x6);
ASSERT_REG_POSITION(semaphore_trigger, 0x7);
ASSERT_REG_POSITION(reference_count, 0x14);
ASSERT_REG_POSITION(semaphore_acquire, 0x1A);
ASSERT_REG_POSITION(semaphore_release, 0x1B);
ASSERT_REG_POSITION(fence_value, 0x1C);
ASSERT_REG_POSITION(fence_action, 0x1D);

ASSERT_REG_POSITION(acquire_mode, 0x100);
ASSERT_REG_POSITION(acquire_source, 0x101);
ASSERT_REG_POSITION(acquire_active, 0x102);
ASSERT_REG_POSITION(acquire_timeout, 0x103);
ASSERT_REG_POSITION(acquire_value, 0x104);

#undef ASSERT_REG_POSITION

} // namespace Tegra
