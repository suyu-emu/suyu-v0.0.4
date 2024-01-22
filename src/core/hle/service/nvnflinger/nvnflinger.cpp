// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <optional>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/service/nvdrv/devices/nvdisp_disp0.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "core/hle/service/nvnflinger/buffer_item_consumer.h"
#include "core/hle/service/nvnflinger/buffer_queue_core.h"
#include "core/hle/service/nvnflinger/fb_share_buffer_manager.h"
#include "core/hle/service/nvnflinger/hos_binder_driver_server.h"
#include "core/hle/service/nvnflinger/nvnflinger.h"
#include "core/hle/service/nvnflinger/ui/graphic_buffer.h"
#include "core/hle/service/vi/display/vi_display.h"
#include "core/hle/service/vi/layer/vi_layer.h"
#include "core/hle/service/vi/vi_results.h"
#include "video_core/gpu.h"
#include "video_core/host1x/host1x.h"
#include "video_core/host1x/syncpoint_manager.h"

namespace Service::Nvnflinger {

constexpr auto frame_ns = std::chrono::nanoseconds{1000000000 / 60};

void Nvnflinger::SplitVSync(std::stop_token stop_token) {
    system.RegisterHostThread();
    std::string name = "VSyncThread";
    MicroProfileOnThreadCreate(name.c_str());

    // Cleanup
    SCOPE_EXIT({ MicroProfileOnThreadExit(); });

    Common::SetCurrentThreadName(name.c_str());
    Common::SetCurrentThreadPriority(Common::ThreadPriority::High);

    while (!stop_token.stop_requested()) {
        vsync_signal.Wait();

        const auto lock_guard = Lock();

        if (!is_abandoned) {
            Compose();
        }
    }
}

Nvnflinger::Nvnflinger(Core::System& system_, HosBinderDriverServer& hos_binder_driver_server_)
    : system(system_), service_context(system_, "nvnflinger"),
      hos_binder_driver_server(hos_binder_driver_server_) {
    displays.emplace_back(0, "Default", hos_binder_driver_server, service_context, system);
    displays.emplace_back(1, "External", hos_binder_driver_server, service_context, system);
    displays.emplace_back(2, "Edid", hos_binder_driver_server, service_context, system);
    displays.emplace_back(3, "Internal", hos_binder_driver_server, service_context, system);
    displays.emplace_back(4, "Null", hos_binder_driver_server, service_context, system);
    guard = std::make_shared<std::mutex>();

    // Schedule the screen composition events
    multi_composition_event = Core::Timing::CreateEvent(
        "ScreenComposition",
        [this](s64 time,
               std::chrono::nanoseconds ns_late) -> std::optional<std::chrono::nanoseconds> {
            vsync_signal.Set();
            return std::chrono::nanoseconds(GetNextTicks());
        });

    single_composition_event = Core::Timing::CreateEvent(
        "ScreenComposition",
        [this](s64 time,
               std::chrono::nanoseconds ns_late) -> std::optional<std::chrono::nanoseconds> {
            const auto lock_guard = Lock();
            Compose();

            return std::chrono::nanoseconds(GetNextTicks());
        });

    if (system.IsMulticore()) {
        system.CoreTiming().ScheduleLoopingEvent(frame_ns, frame_ns, multi_composition_event);
        vsync_thread = std::jthread([this](std::stop_token token) { SplitVSync(token); });
    } else {
        system.CoreTiming().ScheduleLoopingEvent(frame_ns, frame_ns, single_composition_event);
    }
}

Nvnflinger::~Nvnflinger() {
    if (system.IsMulticore()) {
        system.CoreTiming().UnscheduleEvent(multi_composition_event);
        vsync_thread.request_stop();
        vsync_signal.Set();
    } else {
        system.CoreTiming().UnscheduleEvent(single_composition_event);
    }

    ShutdownLayers();

    if (nvdrv) {
        nvdrv->Close(disp_fd);
    }
}

void Nvnflinger::ShutdownLayers() {
    // Abandon consumers.
    {
        const auto lock_guard = Lock();
        for (auto& display : displays) {
            display.Abandon();
        }

        is_abandoned = true;
    }

    // Join the vsync thread, if it exists.
    vsync_thread = {};
}

void Nvnflinger::SetNVDrvInstance(std::shared_ptr<Nvidia::Module> instance) {
    nvdrv = std::move(instance);
    disp_fd = nvdrv->Open("/dev/nvdisp_disp0", {});
}

std::optional<u64> Nvnflinger::OpenDisplay(std::string_view name) {
    const auto lock_guard = Lock();

    LOG_DEBUG(Service_Nvnflinger, "Opening \"{}\" display", name);

    const auto itr =
        std::find_if(displays.begin(), displays.end(),
                     [&](const VI::Display& display) { return display.GetName() == name; });

    if (itr == displays.end()) {
        return std::nullopt;
    }

    return itr->GetID();
}

bool Nvnflinger::CloseDisplay(u64 display_id) {
    const auto lock_guard = Lock();
    auto* const display = FindDisplay(display_id);

    if (display == nullptr) {
        return false;
    }

    display->Reset();

    return true;
}

std::optional<u64> Nvnflinger::CreateLayer(u64 display_id) {
    const auto lock_guard = Lock();
    auto* const display = FindDisplay(display_id);

    if (display == nullptr) {
        return std::nullopt;
    }

    const u64 layer_id = next_layer_id++;
    CreateLayerAtId(*display, layer_id);
    return layer_id;
}

void Nvnflinger::CreateLayerAtId(VI::Display& display, u64 layer_id) {
    const auto buffer_id = next_buffer_queue_id++;
    display.CreateLayer(layer_id, buffer_id, nvdrv->container);
}

bool Nvnflinger::OpenLayer(u64 layer_id) {
    const auto lock_guard = Lock();

    for (auto& display : displays) {
        if (auto* layer = display.FindLayer(layer_id); layer) {
            return layer->Open();
        }
    }

    return false;
}

bool Nvnflinger::CloseLayer(u64 layer_id) {
    const auto lock_guard = Lock();

    for (auto& display : displays) {
        if (auto* layer = display.FindLayer(layer_id); layer) {
            return layer->Close();
        }
    }

    return false;
}

void Nvnflinger::DestroyLayer(u64 layer_id) {
    const auto lock_guard = Lock();

    for (auto& display : displays) {
        display.DestroyLayer(layer_id);
    }
}

std::optional<u32> Nvnflinger::FindBufferQueueId(u64 display_id, u64 layer_id) {
    const auto lock_guard = Lock();
    const auto* const layer = FindLayer(display_id, layer_id);

    if (layer == nullptr) {
        return std::nullopt;
    }

    return layer->GetBinderId();
}

Result Nvnflinger::FindVsyncEvent(Kernel::KReadableEvent** out_vsync_event, u64 display_id) {
    const auto lock_guard = Lock();
    auto* const display = FindDisplay(display_id);

    if (display == nullptr) {
        return VI::ResultNotFound;
    }

    *out_vsync_event = display->GetVSyncEvent();
    return ResultSuccess;
}

VI::Display* Nvnflinger::FindDisplay(u64 display_id) {
    const auto itr =
        std::find_if(displays.begin(), displays.end(),
                     [&](const VI::Display& display) { return display.GetID() == display_id; });

    if (itr == displays.end()) {
        return nullptr;
    }

    return &*itr;
}

const VI::Display* Nvnflinger::FindDisplay(u64 display_id) const {
    const auto itr =
        std::find_if(displays.begin(), displays.end(),
                     [&](const VI::Display& display) { return display.GetID() == display_id; });

    if (itr == displays.end()) {
        return nullptr;
    }

    return &*itr;
}

VI::Layer* Nvnflinger::FindLayer(u64 display_id, u64 layer_id) {
    auto* const display = FindDisplay(display_id);

    if (display == nullptr) {
        return nullptr;
    }

    return display->FindLayer(layer_id);
}

void Nvnflinger::Compose() {
    for (auto& display : displays) {
        // Trigger vsync for this display at the end of drawing
        SCOPE_EXIT({ display.SignalVSyncEvent(); });

        // Don't do anything for displays without layers.
        if (!display.HasLayers())
            continue;

        // TODO(Subv): Support more than 1 layer.
        VI::Layer& layer = display.GetLayer(0);

        android::BufferItem buffer{};
        const auto status = layer.GetConsumer().AcquireBuffer(&buffer, {}, false);

        if (status != android::Status::NoError) {
            continue;
        }

        const auto& igbp_buffer = *buffer.graphic_buffer;

        if (!system.IsPoweredOn()) {
            return; // We are likely shutting down
        }

        // Now send the buffer to the GPU for drawing.
        // TODO(Subv): Support more than just disp0. The display device selection is probably based
        // on which display we're drawing (Default, Internal, External, etc)
        auto nvdisp = nvdrv->GetDevice<Nvidia::Devices::nvdisp_disp0>(disp_fd);
        ASSERT(nvdisp);

        Common::Rectangle<int> crop_rect{
            static_cast<int>(buffer.crop.Left()), static_cast<int>(buffer.crop.Top()),
            static_cast<int>(buffer.crop.Right()), static_cast<int>(buffer.crop.Bottom())};

        nvdisp->flip(igbp_buffer.BufferId(), igbp_buffer.Offset(), igbp_buffer.ExternalFormat(),
                     igbp_buffer.Width(), igbp_buffer.Height(), igbp_buffer.Stride(),
                     static_cast<android::BufferTransformFlags>(buffer.transform), crop_rect,
                     buffer.fence.fences, buffer.fence.num_fences);

        MicroProfileFlip();

        swap_interval = buffer.swap_interval;

        layer.GetConsumer().ReleaseBuffer(buffer, android::Fence::NoFence());
    }
}

s64 Nvnflinger::GetNextTicks() const {
    const auto& settings = Settings::values;
    auto speed_scale = 1.f;
    if (settings.use_multi_core.GetValue()) {
        if (settings.use_speed_limit.GetValue()) {
            // Scales the speed based on speed_limit setting on MC. SC is handled by
            // SpeedLimiter::DoSpeedLimiting.
            speed_scale = 100.f / settings.speed_limit.GetValue();
        } else {
            // Run at unlocked framerate.
            speed_scale = 0.01f;
        }
    }
    if (system.GetNVDECActive() && settings.use_video_framerate.GetValue()) {
        // Run at intended presentation rate during video playback.
        speed_scale = 1.f;
    }

    // As an extension, treat nonpositive swap interval as framerate multiplier.
    const f32 effective_fps = swap_interval <= 0 ? 120.f * static_cast<f32>(1 - swap_interval)
                                                 : 60.f / static_cast<f32>(swap_interval);

    return static_cast<s64>(speed_scale * (1000000000.f / effective_fps));
}

FbShareBufferManager& Nvnflinger::GetSystemBufferManager() {
    const auto lock_guard = Lock();

    if (!system_buffer_manager) {
        system_buffer_manager = std::make_unique<FbShareBufferManager>(system, *this, nvdrv);
    }

    return *system_buffer_manager;
}

} // namespace Service::Nvnflinger
