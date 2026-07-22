// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <list>
#include <memory>

#include "common/assert.h"
#include "common/settings.h"
#include "common/settings_enums.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/frontend/emu_window.h"
#include "core/frontend/graphics_context.h"
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
#include "video_core/host1x/host1x.h"
#include "video_core/host1x/syncpoint_manager.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_base.h"
#include "video_core/shader_notify.h"

namespace Tegra {

struct GPU::Impl {
    explicit Impl(Core::System& system_, bool is_async_, bool use_nvdec_)
        : system{system_}
        , use_nvdec{use_nvdec_}
        , shader_notify()
        , is_async{is_async_}
        , gpu_thread{system_}
    {}

    ~Impl() = default;

    std::shared_ptr<Control::ChannelState> CreateChannel(s32 channel_id) {
        auto channel_state = std::make_shared<Tegra::Control::ChannelState>(channel_id);
        channels.emplace(channel_id, channel_state);
        scheduler.DeclareChannel(channel_state);
        return channel_state;
    }

    void BindChannel(s32 channel_id) {
        if (bound_channel != channel_id) {
            auto it = channels.find(channel_id);
            ASSERT(it != channels.end());
            bound_channel = channel_id;
            current_channel = it->second.get();
            renderer->ReadRasterizer()->BindChannel(*current_channel);
        }
    }

    std::shared_ptr<Control::ChannelState> AllocateChannel() {
        return CreateChannel(new_channel_id++);
    }

    void InitChannel(Control::ChannelState& to_init, u64 program_id) {
        to_init.Init(system, program_id);
        to_init.BindRasterizer(renderer->ReadRasterizer());
        renderer->ReadRasterizer()->InitializeChannel(to_init);
    }

    void InitAddressSpace(Tegra::MemoryManager& memory_manager) {
        memory_manager.BindRasterizer(renderer->ReadRasterizer());
    }

    void ReleaseChannel(Control::ChannelState& to_release) {
        UNIMPLEMENTED();
    }

    /// Binds a renderer to the GPU.
    void BindRenderer(std::unique_ptr<VideoCore::RendererBase> renderer_) {
        renderer = std::move(renderer_);
        system.Host1x().memory_manager.BindInterface(renderer->ReadRasterizer());
        system.Host1x().gmmu_manager.BindRasterizer(renderer->ReadRasterizer());
    }

    /// Flush all current written commands into the host GPU for execution.
    void FlushCommands() {
        renderer->ReadRasterizer()->FlushCommands();
    }

    /// Synchronizes CPU writes with Host GPU memory.
    void InvalidateGPUCache() {
        std::function<void(PAddr, size_t)> callback_writes([this](PAddr address, size_t size) {
            renderer->ReadRasterizer()->OnCacheInvalidation(address, size);
        });
        system.GatherGPUDirtyMemory(callback_writes);
    }

    /// Signal the ending of command list.
    void OnCommandListEnd() {
        renderer->ReadRasterizer()->ReleaseFences(false);
        Settings::UpdateGPUAccuracy();
    }

    /// Request a host GPU memory flush from the CPU.
    template <typename Func>
    [[nodiscard]] u64 RequestSyncOperation(Func&& action) {
        std::unique_lock lck{sync_request_mutex};
        const u64 fence = ++last_sync_fence;
        sync_requests.emplace_back(action);
        return fence;
    }

    /// Obtains current flush request fence id.
    [[nodiscard]] u64 CurrentSyncRequestFence() const {
        return current_sync_fence.load(std::memory_order_relaxed);
    }

    void WaitForSyncOperation(const u64 fence) {
        std::unique_lock lck{sync_request_mutex};
        sync_request_cv.wait(lck, [this, fence] { return CurrentSyncRequestFence() >= fence; });
    }

    /// Tick pending requests within the GPU.
    void TickWork() {
        std::unique_lock lck{sync_request_mutex};
        while (!sync_requests.empty()) {
            auto request = std::move(sync_requests.front());
            sync_requests.pop_front();
            sync_request_mutex.unlock();
            request();
            current_sync_fence.fetch_add(1, std::memory_order_release);
            sync_request_mutex.lock();
            sync_request_cv.notify_all();
        }
    }

    [[nodiscard]] u64 GetTicks() const {
        u64 gpu_tick = system.CoreTiming().GetGPUTicks();
        Settings::GpuOverclock overclock = Settings::values.fast_gpu_time.GetValue();

        if (overclock != Settings::GpuOverclock::Normal) {
            gpu_tick /= 256 * u64(overclock);
        }

        return gpu_tick;
    }

    void RendererFrameEndNotify() {
        system.GetPerfStats().EndGameFrame();
    }

    /// Performs any additional setup necessary in order to begin GPU emulation.
    /// This can be used to launch any necessary threads and register any necessary
    /// core timing events.
    void Start() {
        Settings::UpdateGPUAccuracy();
        gpu_thread.StartThread(*renderer, renderer->Context(), scheduler);
    }

    void NotifyShutdown() {
        std::unique_lock lk{sync_mutex};
        shutting_down.store(true, std::memory_order::relaxed);
        sync_cv.notify_all();
    }

    /// Obtain the CPU Context
    void ObtainContext() {
        if (!cpu_context) {
            cpu_context = renderer->GetRenderWindow().CreateSharedContext();
        }
        cpu_context->MakeCurrent();
    }

    /// Release the CPU Context
    void ReleaseContext() {
        cpu_context->DoneCurrent();
    }

    /// Push GPU command entries to be processed
    void PushGPUEntries(s32 channel, Tegra::CommandList&& entries) {
        gpu_thread.SubmitList(channel, std::move(entries), is_async);
    }

    /// Notify rasterizer that any caches of the specified region should be flushed to Switch memory
    void FlushRegion(DAddr addr, u64 size) {
        gpu_thread.FlushRegion(addr, size, is_async);
    }

    VideoCore::RasterizerDownloadArea OnCPURead(DAddr addr, u64 size) {
        auto raster_area = renderer->ReadRasterizer()->GetFlushArea(addr, size);
        if (raster_area.preemtive) {
            return raster_area;
        }
        raster_area.preemtive = true;
        const u64 fence = RequestSyncOperation([this, &raster_area]() {
            renderer->ReadRasterizer()->FlushRegion(raster_area.start_address, raster_area.end_address - raster_area.start_address);
        });
        gpu_thread.TickGPU(is_async);
        WaitForSyncOperation(fence);
        return raster_area;
    }

    /// Notify rasterizer that any caches of the specified region should be invalidated
    void InvalidateRegion(DAddr addr, u64 size) {
        gpu_thread.InvalidateRegion(addr, size);
    }

    bool OnCPUWrite(DAddr addr, u64 size) {
        return renderer->ReadRasterizer()->OnCPUWrite(addr, size);
    }

    /// Notify rasterizer that any caches of the specified region should be flushed and invalidated
    void FlushAndInvalidateRegion(DAddr addr, u64 size) {
        gpu_thread.FlushAndInvalidateRegion(addr, size, is_async);
    }

    void RequestComposite(std::vector<Tegra::FramebufferConfig>&& layers, std::vector<Service::Nvidia::NvFence>&& fences) {
        size_t num_fences{fences.size()};
        size_t current_request_counter{};
        {
            std::unique_lock<std::mutex> lk(request_swap_mutex);
            if (free_swap_counters.empty()) {
                current_request_counter = request_swap_counters.size();
                request_swap_counters.emplace_back(num_fences);
            } else {
                current_request_counter = free_swap_counters.front();
                request_swap_counters[current_request_counter] = num_fences;
                free_swap_counters.pop_front();
            }
        }
        const auto wait_fence = RequestSyncOperation([this, current_request_counter, &layers, &fences, num_fences] {
            auto& syncpoint_manager = system.Host1x().GetSyncpointManager();
            if (num_fences == 0) {
                renderer->Composite(layers);
            }
            const auto executer = [this, current_request_counter, layers_copy = layers]() {
                {
                    std::unique_lock<std::mutex> lk(request_swap_mutex);
                    if (--request_swap_counters[current_request_counter] != 0) {
                        return;
                    }
                    free_swap_counters.push_back(current_request_counter);
                }
                renderer->Composite(layers_copy);
            };
            for (size_t i = 0; i < num_fences; i++) {
                syncpoint_manager.RegisterGuestAction(fences[i].id, fences[i].value, executer);
            }
        });
        gpu_thread.TickGPU(is_async);
        WaitForSyncOperation(wait_fence);
    }

    std::vector<u8> GetAppletCaptureBuffer() {
        std::vector<u8> out;

        const auto wait_fence =
            RequestSyncOperation([&] { out = renderer->GetAppletCaptureBuffer(); });
        gpu_thread.TickGPU(is_async);
        WaitForSyncOperation(wait_fence);

        return out;
    }

    Core::System& system;

    std::unique_ptr<VideoCore::RendererBase> renderer;
    const bool use_nvdec;

    s32 new_channel_id{1};
    /// Shader build notifier
    VideoCore::ShaderNotify shader_notify;
    /// When true, we are about to shut down emulation session, so terminate outstanding tasks
    std::atomic_bool shutting_down{};

    std::array<std::atomic<u32>, Service::Nvidia::MaxSyncPoints> syncpoints{};

    std::array<std::list<u32>, Service::Nvidia::MaxSyncPoints> syncpt_interrupts;

    std::mutex sync_mutex;
    std::mutex device_mutex;

    std::condition_variable sync_cv;

    std::list<std::function<void()>> sync_requests;
    std::atomic<u64> current_sync_fence{};
    u64 last_sync_fence{};
    std::mutex sync_request_mutex;
    std::condition_variable sync_request_cv;

    const bool is_async;

    VideoCommon::GPUThread::ThreadManager gpu_thread;
    std::unique_ptr<Core::Frontend::GraphicsContext> cpu_context;

    Tegra::Control::Scheduler scheduler;
    ankerl::unordered_dense::map<s32, std::shared_ptr<Tegra::Control::ChannelState>> channels;
    Tegra::Control::ChannelState* current_channel;
    s32 bound_channel{-1};

    std::deque<size_t> free_swap_counters;
    std::deque<size_t> request_swap_counters;
    std::mutex request_swap_mutex;
};

GPU::GPU(Core::System& system, bool is_async, bool use_nvdec)
    : impl{std::make_unique<Impl>(system, is_async, use_nvdec)}
{}

GPU::~GPU() = default;

std::shared_ptr<Control::ChannelState> GPU::AllocateChannel() {
    return impl->AllocateChannel();
}

void GPU::InitChannel(Control::ChannelState& to_init, u64 program_id) {
    impl->InitChannel(to_init, program_id);
}

void GPU::BindChannel(s32 channel_id) {
    impl->BindChannel(channel_id);
}

void GPU::ReleaseChannel(Control::ChannelState& to_release) {
    impl->ReleaseChannel(to_release);
}

void GPU::InitAddressSpace(Tegra::MemoryManager& memory_manager) {
    impl->InitAddressSpace(memory_manager);
}

void GPU::BindRenderer(std::unique_ptr<VideoCore::RendererBase> renderer) {
    impl->BindRenderer(std::move(renderer));
}

void GPU::FlushCommands() {
    impl->FlushCommands();
}

void GPU::InvalidateGPUCache() {
    impl->InvalidateGPUCache();
}

void GPU::OnCommandListEnd() {
    impl->OnCommandListEnd();
}

u64 GPU::RequestFlush(DAddr addr, std::size_t size) {
    return impl->RequestSyncOperation([this, addr, size]() {
        impl->renderer->ReadRasterizer()->FlushRegion(addr, size);
    });
}

u64 GPU::CurrentSyncRequestFence() const {
    return impl->CurrentSyncRequestFence();
}

void GPU::WaitForSyncOperation(u64 fence) {
    return impl->WaitForSyncOperation(fence);
}

void GPU::TickWork() {
    impl->TickWork();
}

/// Gets a mutable reference to the Host1x interface
Host1x::Host1x& GPU::Host1x() {
    return impl->system.Host1x();
}

/// Gets an immutable reference to the Host1x interface.
const Host1x::Host1x& GPU::Host1x() const {
    return impl->system.Host1x();
}

Engines::Maxwell3D& GPU::Maxwell3D() {
    return impl->current_channel->payload->maxwell_3d;
}

const Engines::Maxwell3D& GPU::Maxwell3D() const {
    return impl->current_channel->payload->maxwell_3d;
}

Engines::KeplerCompute& GPU::KeplerCompute() {
    return impl->current_channel->payload->kepler_compute;
}

const Engines::KeplerCompute& GPU::KeplerCompute() const {
    return impl->current_channel->payload->kepler_compute;
}

Tegra::DmaPusher& GPU::DmaPusher() {
    return impl->current_channel->payload->dma_pusher;
}

const Tegra::DmaPusher& GPU::DmaPusher() const {
    return impl->current_channel->payload->dma_pusher;
}

VideoCore::RendererBase& GPU::Renderer() {
    return *impl->renderer;
}

const VideoCore::RendererBase& GPU::Renderer() const {
    return *impl->renderer;
}

VideoCore::ShaderNotify& GPU::ShaderNotify() {
    return impl->shader_notify;
}

const VideoCore::ShaderNotify& GPU::ShaderNotify() const {
    return impl->shader_notify;
}

void GPU::RequestComposite(std::vector<Tegra::FramebufferConfig>&& layers,
                           std::vector<Service::Nvidia::NvFence>&& fences) {
    impl->RequestComposite(std::move(layers), std::move(fences));
}

std::vector<u8> GPU::GetAppletCaptureBuffer() {
    return impl->GetAppletCaptureBuffer();
}

u64 GPU::GetTicks() const {
    return impl->GetTicks();
}

bool GPU::IsAsync() const {
    return impl->is_async;
}

bool GPU::UseNvdec() const {
    return impl->use_nvdec;
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

VideoCore::RasterizerDownloadArea GPU::OnCPURead(PAddr addr, u64 size) {
    return impl->OnCPURead(addr, size);
}

void GPU::FlushRegion(DAddr addr, u64 size) {
    impl->FlushRegion(addr, size);
}

void GPU::InvalidateRegion(DAddr addr, u64 size) {
    impl->InvalidateRegion(addr, size);
}

bool GPU::OnCPUWrite(DAddr addr, u64 size) {
    return impl->OnCPUWrite(addr, size);
}

void GPU::FlushAndInvalidateRegion(DAddr addr, u64 size) {
    impl->FlushAndInvalidateRegion(addr, size);
}

} // namespace Tegra
