// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <list>
#include <memory>

#include "common/assert.h"
#include "common/microprofile.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/frontend/emu_window.h"
#include "core/hardware_interrupt_manager.h"
#include "core/hle/service/nvdrv/nvdata.h"
#include "core/perf_stats.h"
#include "video_core/cdma_pusher.h"
#include "video_core/control/channel_state.h"
#include "video_core/control/scheduler.h"
#include "video_core/dma_pusher.h"
#include "video_core/engines/fermi_2d.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/kepler_memory.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/maxwell_dma.h"
#include "video_core/gpu.h"
#include "video_core/gpu_thread.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_base.h"
#include "video_core/shader_notify.h"

namespace Tegra {

MICROPROFILE_DEFINE(GPU_wait, "GPU", "Wait for the GPU", MP_RGB(128, 128, 192));

struct GPU::Impl {
    explicit Impl(GPU& gpu_, Core::System& system_, bool is_async_, bool use_nvdec_)
        : gpu{gpu_}, system{system_}, use_nvdec{use_nvdec_},
          shader_notify{std::make_unique<VideoCore::ShaderNotify>()}, is_async{is_async_},
          gpu_thread{system_, is_async_}, scheduler{std::make_unique<Control::Scheduler>(gpu)} {}

    ~Impl() = default;

    std::shared_ptr<Control::ChannelState> CreateChannel(s32 channel_id) {
        auto channel_state = std::make_shared<Tegra::Control::ChannelState>(channel_id);
        channels.emplace(channel_id, channel_state);
        scheduler->DeclareChannel(channel_state);
        return channel_state;
    }

    void BindChannel(s32 channel_id) {
        if (bound_channel == channel_id) {
            return;
        }
        auto it = channels.find(channel_id);
        ASSERT(it != channels.end());
        bound_channel = channel_id;
        current_channel = it->second.get();

        rasterizer->BindChannel(*current_channel);
    }

    std::shared_ptr<Control::ChannelState> AllocateChannel() {
        return CreateChannel(new_channel_id++);
    }

    void InitChannel(Control::ChannelState& to_init) {
        to_init.Init(system, gpu);
        to_init.BindRasterizer(rasterizer);
        rasterizer->InitializeChannel(to_init);
    }

    void ReleaseChannel(Control::ChannelState& to_release) {
        UNIMPLEMENTED();
    }

    void CreateHost1xChannel() {
        if (host1x_channel) {
            return;
        }
        host1x_channel = CreateChannel(0);
        host1x_channel->memory_manager = std::make_shared<Tegra::MemoryManager>(system);
        InitChannel(*host1x_channel);
    }

    /// Binds a renderer to the GPU.
    void BindRenderer(std::unique_ptr<VideoCore::RendererBase> renderer_) {
        renderer = std::move(renderer_);
        rasterizer = renderer->ReadRasterizer();
    }

    /// Flush all current written commands into the host GPU for execution.
    void FlushCommands() {
        rasterizer->FlushCommands();
    }

    /// Synchronizes CPU writes with Host GPU memory.
    void SyncGuestHost() {
        rasterizer->SyncGuestHost();
    }

    /// Signal the ending of command list.
    void OnCommandListEnd() {
        if (is_async) {
            // This command only applies to asynchronous GPU mode
            gpu_thread.OnCommandListEnd();
        }
    }

    /// Request a host GPU memory flush from the CPU.
    [[nodiscard]] u64 RequestFlush(VAddr addr, std::size_t size) {
        std::unique_lock lck{flush_request_mutex};
        const u64 fence = ++last_flush_fence;
        flush_requests.emplace_back(fence, addr, size);
        return fence;
    }

    /// Obtains current flush request fence id.
    [[nodiscard]] u64 CurrentFlushRequestFence() const {
        return current_flush_fence.load(std::memory_order_relaxed);
    }

    /// Tick pending requests within the GPU.
    void TickWork() {
        std::unique_lock lck{flush_request_mutex};
        while (!flush_requests.empty()) {
            auto& request = flush_requests.front();
            const u64 fence = request.fence;
            const VAddr addr = request.addr;
            const std::size_t size = request.size;
            flush_requests.pop_front();
            flush_request_mutex.unlock();
            rasterizer->FlushRegion(addr, size);
            current_flush_fence.store(fence);
            flush_request_mutex.lock();
        }
    }

    /// Returns a reference to the Maxwell3D GPU engine.
    [[nodiscard]] Engines::Maxwell3D& Maxwell3D() {
        ASSERT(current_channel);
        return *current_channel->maxwell_3d;
    }

    /// Returns a const reference to the Maxwell3D GPU engine.
    [[nodiscard]] const Engines::Maxwell3D& Maxwell3D() const {
        ASSERT(current_channel);
        return *current_channel->maxwell_3d;
    }

    /// Returns a reference to the KeplerCompute GPU engine.
    [[nodiscard]] Engines::KeplerCompute& KeplerCompute() {
        ASSERT(current_channel);
        return *current_channel->kepler_compute;
    }

    /// Returns a reference to the KeplerCompute GPU engine.
    [[nodiscard]] const Engines::KeplerCompute& KeplerCompute() const {
        ASSERT(current_channel);
        return *current_channel->kepler_compute;
    }

    /// Returns a reference to the GPU memory manager.
    [[nodiscard]] Tegra::MemoryManager& MemoryManager() {
        CreateHost1xChannel();
        return *host1x_channel->memory_manager;
    }

    /// Returns a reference to the GPU DMA pusher.
    [[nodiscard]] Tegra::DmaPusher& DmaPusher() {
        ASSERT(current_channel);
        return *current_channel->dma_pusher;
    }

    /// Returns a const reference to the GPU DMA pusher.
    [[nodiscard]] const Tegra::DmaPusher& DmaPusher() const {
        ASSERT(current_channel);
        return *current_channel->dma_pusher;
    }

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

    /// Allows the CPU/NvFlinger to wait on the GPU before presenting a frame.
    void WaitFence(u32 syncpoint_id, u32 value) {
        // Synced GPU, is always in sync
        if (!is_async) {
            return;
        }
        if (syncpoint_id == UINT32_MAX) {
            // TODO: Research what this does.
            LOG_ERROR(HW_GPU, "Waiting for syncpoint -1 not implemented");
            return;
        }
        MICROPROFILE_SCOPE(GPU_wait);
        std::unique_lock lock{sync_mutex};
        sync_cv.wait(lock, [=, this] {
            if (shutting_down.load(std::memory_order_relaxed)) {
                // We're shutting down, ensure no threads continue to wait for the next syncpoint
                return true;
            }
            return syncpoints.at(syncpoint_id).load() >= value;
        });
    }

    void IncrementSyncPoint(u32 syncpoint_id) {
        auto& syncpoint = syncpoints.at(syncpoint_id);
        syncpoint++;
        std::scoped_lock lock{sync_mutex};
        sync_cv.notify_all();
        auto& interrupt = syncpt_interrupts.at(syncpoint_id);
        if (!interrupt.empty()) {
            u32 value = syncpoint.load();
            auto it = interrupt.begin();
            while (it != interrupt.end()) {
                if (value >= *it) {
                    TriggerCpuInterrupt(syncpoint_id, *it);
                    it = interrupt.erase(it);
                    continue;
                }
                it++;
            }
        }
    }

    [[nodiscard]] u32 GetSyncpointValue(u32 syncpoint_id) const {
        return syncpoints.at(syncpoint_id).load();
    }

    void RegisterSyncptInterrupt(u32 syncpoint_id, u32 value) {
        std::scoped_lock lock{sync_mutex};
        auto& interrupt = syncpt_interrupts.at(syncpoint_id);
        bool contains = std::any_of(interrupt.begin(), interrupt.end(),
                                    [value](u32 in_value) { return in_value == value; });
        if (contains) {
            return;
        }
        interrupt.emplace_back(value);
    }

    [[nodiscard]] bool CancelSyncptInterrupt(u32 syncpoint_id, u32 value) {
        std::scoped_lock lock{sync_mutex};
        auto& interrupt = syncpt_interrupts.at(syncpoint_id);
        const auto iter =
            std::find_if(interrupt.begin(), interrupt.end(),
                         [value](u32 interrupt_value) { return value == interrupt_value; });

        if (iter == interrupt.end()) {
            return false;
        }
        interrupt.erase(iter);
        return true;
    }

    [[nodiscard]] u64 GetTicks() const {
        // This values were reversed engineered by fincs from NVN
        // The gpu clock is reported in units of 385/625 nanoseconds
        constexpr u64 gpu_ticks_num = 384;
        constexpr u64 gpu_ticks_den = 625;

        u64 nanoseconds = system.CoreTiming().GetGlobalTimeNs().count();
        if (Settings::values.use_fast_gpu_time.GetValue()) {
            nanoseconds /= 256;
        }
        const u64 nanoseconds_num = nanoseconds / gpu_ticks_den;
        const u64 nanoseconds_rem = nanoseconds % gpu_ticks_den;
        return nanoseconds_num * gpu_ticks_num + (nanoseconds_rem * gpu_ticks_num) / gpu_ticks_den;
    }

    [[nodiscard]] bool IsAsync() const {
        return is_async;
    }

    [[nodiscard]] bool UseNvdec() const {
        return use_nvdec;
    }

    void RendererFrameEndNotify() {
        system.GetPerfStats().EndGameFrame();
    }

    /// Performs any additional setup necessary in order to begin GPU emulation.
    /// This can be used to launch any necessary threads and register any necessary
    /// core timing events.
    void Start() {
        gpu_thread.StartThread(*renderer, renderer->Context(), *scheduler);
        cpu_context = renderer->GetRenderWindow().CreateSharedContext();
        cpu_context->MakeCurrent();
    }

    void NotifyShutdown() {
        std::unique_lock lk{sync_mutex};
        shutting_down.store(true, std::memory_order::relaxed);
        sync_cv.notify_all();
    }

    /// Obtain the CPU Context
    void ObtainContext() {
        cpu_context->MakeCurrent();
    }

    /// Release the CPU Context
    void ReleaseContext() {
        cpu_context->DoneCurrent();
    }

    /// Push GPU command entries to be processed
    void PushGPUEntries(s32 channel, Tegra::CommandList&& entries) {
        gpu_thread.SubmitList(channel, std::move(entries));
    }

    /// Push GPU command buffer entries to be processed
    void PushCommandBuffer(u32 id, Tegra::ChCommandHeaderList& entries) {
        if (!use_nvdec) {
            return;
        }

        if (!cdma_pushers.contains(id)) {
            cdma_pushers.insert_or_assign(id, std::make_unique<Tegra::CDmaPusher>(gpu));
        }

        // SubmitCommandBuffer would make the nvdec operations async, this is not currently working
        // TODO(ameerj): RE proper async nvdec operation
        // gpu_thread.SubmitCommandBuffer(std::move(entries));
        cdma_pushers[id]->ProcessEntries(std::move(entries));
    }

    /// Frees the CDMAPusher instance to free up resources
    void ClearCdmaInstance(u32 id) {
        const auto iter = cdma_pushers.find(id);
        if (iter != cdma_pushers.end()) {
            cdma_pushers.erase(iter);
        }
    }

    /// Swap buffers (render frame)
    void SwapBuffers(const Tegra::FramebufferConfig* framebuffer) {
        gpu_thread.SwapBuffers(framebuffer);
    }

    /// Notify rasterizer that any caches of the specified region should be flushed to Switch memory
    void FlushRegion(VAddr addr, u64 size) {
        gpu_thread.FlushRegion(addr, size);
    }

    /// Notify rasterizer that any caches of the specified region should be invalidated
    void InvalidateRegion(VAddr addr, u64 size) {
        gpu_thread.InvalidateRegion(addr, size);
    }

    /// Notify rasterizer that any caches of the specified region should be flushed and invalidated
    void FlushAndInvalidateRegion(VAddr addr, u64 size) {
        gpu_thread.FlushAndInvalidateRegion(addr, size);
    }

    void TriggerCpuInterrupt(u32 syncpoint_id, u32 value) const {
        auto& interrupt_manager = system.InterruptManager();
        interrupt_manager.GPUInterruptSyncpt(syncpoint_id, value);
    }

    GPU& gpu;
    Core::System& system;

    std::map<u32, std::unique_ptr<Tegra::CDmaPusher>> cdma_pushers;
    std::unique_ptr<VideoCore::RendererBase> renderer;
    VideoCore::RasterizerInterface* rasterizer = nullptr;
    const bool use_nvdec;

    std::shared_ptr<Control::ChannelState> host1x_channel;
    s32 new_channel_id{1};
    /// Shader build notifier
    std::unique_ptr<VideoCore::ShaderNotify> shader_notify;
    /// When true, we are about to shut down emulation session, so terminate outstanding tasks
    std::atomic_bool shutting_down{};

    std::array<std::atomic<u32>, Service::Nvidia::MaxSyncPoints> syncpoints{};

    std::array<std::list<u32>, Service::Nvidia::MaxSyncPoints> syncpt_interrupts;

    std::mutex sync_mutex;
    std::mutex device_mutex;

    std::condition_variable sync_cv;

    struct FlushRequest {
        explicit FlushRequest(u64 fence_, VAddr addr_, std::size_t size_)
            : fence{fence_}, addr{addr_}, size{size_} {}
        u64 fence;
        VAddr addr;
        std::size_t size;
    };

    std::list<FlushRequest> flush_requests;
    std::atomic<u64> current_flush_fence{};
    u64 last_flush_fence{};
    std::mutex flush_request_mutex;

    const bool is_async;

    VideoCommon::GPUThread::ThreadManager gpu_thread;
    std::unique_ptr<Core::Frontend::GraphicsContext> cpu_context;

    std::unique_ptr<Tegra::Control::Scheduler> scheduler;
    std::unordered_map<s32, std::shared_ptr<Tegra::Control::ChannelState>> channels;
    Tegra::Control::ChannelState* current_channel;
    s32 bound_channel{-1};
};

GPU::GPU(Core::System& system, bool is_async, bool use_nvdec)
    : impl{std::make_unique<Impl>(*this, system, is_async, use_nvdec)} {}

GPU::~GPU() = default;

std::shared_ptr<Control::ChannelState> GPU::AllocateChannel() {
    return impl->AllocateChannel();
}

void GPU::InitChannel(Control::ChannelState& to_init) {
    impl->InitChannel(to_init);
}

void GPU::BindChannel(s32 channel_id) {
    impl->BindChannel(channel_id);
}

void GPU::ReleaseChannel(Control::ChannelState& to_release) {
    impl->ReleaseChannel(to_release);
}

void GPU::BindRenderer(std::unique_ptr<VideoCore::RendererBase> renderer) {
    impl->BindRenderer(std::move(renderer));
}

void GPU::FlushCommands() {
    impl->FlushCommands();
}

void GPU::SyncGuestHost() {
    impl->SyncGuestHost();
}

void GPU::OnCommandListEnd() {
    impl->OnCommandListEnd();
}

u64 GPU::RequestFlush(VAddr addr, std::size_t size) {
    return impl->RequestFlush(addr, size);
}

u64 GPU::CurrentFlushRequestFence() const {
    return impl->CurrentFlushRequestFence();
}

void GPU::TickWork() {
    impl->TickWork();
}

Engines::Maxwell3D& GPU::Maxwell3D() {
    return impl->Maxwell3D();
}

const Engines::Maxwell3D& GPU::Maxwell3D() const {
    return impl->Maxwell3D();
}

Engines::KeplerCompute& GPU::KeplerCompute() {
    return impl->KeplerCompute();
}

const Engines::KeplerCompute& GPU::KeplerCompute() const {
    return impl->KeplerCompute();
}

Tegra::MemoryManager& GPU::MemoryManager() {
    return impl->MemoryManager();
}

const Tegra::MemoryManager& GPU::MemoryManager() const {
    return impl->MemoryManager();
}

Tegra::DmaPusher& GPU::DmaPusher() {
    return impl->DmaPusher();
}

const Tegra::DmaPusher& GPU::DmaPusher() const {
    return impl->DmaPusher();
}

VideoCore::RendererBase& GPU::Renderer() {
    return impl->Renderer();
}

const VideoCore::RendererBase& GPU::Renderer() const {
    return impl->Renderer();
}

VideoCore::ShaderNotify& GPU::ShaderNotify() {
    return impl->ShaderNotify();
}

const VideoCore::ShaderNotify& GPU::ShaderNotify() const {
    return impl->ShaderNotify();
}

void GPU::WaitFence(u32 syncpoint_id, u32 value) {
    impl->WaitFence(syncpoint_id, value);
}

void GPU::IncrementSyncPoint(u32 syncpoint_id) {
    impl->IncrementSyncPoint(syncpoint_id);
}

u32 GPU::GetSyncpointValue(u32 syncpoint_id) const {
    return impl->GetSyncpointValue(syncpoint_id);
}

void GPU::RegisterSyncptInterrupt(u32 syncpoint_id, u32 value) {
    impl->RegisterSyncptInterrupt(syncpoint_id, value);
}

bool GPU::CancelSyncptInterrupt(u32 syncpoint_id, u32 value) {
    return impl->CancelSyncptInterrupt(syncpoint_id, value);
}

u64 GPU::GetTicks() const {
    return impl->GetTicks();
}

bool GPU::IsAsync() const {
    return impl->IsAsync();
}

bool GPU::UseNvdec() const {
    return impl->UseNvdec();
}

void GPU::RendererFrameEndNotify() {
    impl->RendererFrameEndNotify();
}

void GPU::Start() {
    impl->Start();
}

void GPU::NotifyShutdown() {
    impl->NotifyShutdown();
}

void GPU::ObtainContext() {
    impl->ObtainContext();
}

void GPU::ReleaseContext() {
    impl->ReleaseContext();
}

void GPU::PushGPUEntries(s32 channel, Tegra::CommandList&& entries) {
    impl->PushGPUEntries(channel, std::move(entries));
}

void GPU::PushCommandBuffer(u32 id, Tegra::ChCommandHeaderList& entries) {
    impl->PushCommandBuffer(id, entries);
}

void GPU::ClearCdmaInstance(u32 id) {
    impl->ClearCdmaInstance(id);
}

void GPU::SwapBuffers(const Tegra::FramebufferConfig* framebuffer) {
    impl->SwapBuffers(framebuffer);
}

void GPU::FlushRegion(VAddr addr, u64 size) {
    impl->FlushRegion(addr, size);
}

void GPU::InvalidateRegion(VAddr addr, u64 size) {
    impl->InvalidateRegion(addr, size);
}

void GPU::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    impl->FlushAndInvalidateRegion(addr, size);
}

} // namespace Tegra
