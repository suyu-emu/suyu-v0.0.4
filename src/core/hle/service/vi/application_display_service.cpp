// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/string_util.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/nvnflinger/nvnflinger.h"
#include "core/hle/service/nvnflinger/parcel.h"
#include "core/hle/service/vi/application_display_service.h"
#include "core/hle/service/vi/hos_binder_driver.h"
#include "core/hle/service/vi/manager_display_service.h"
#include "core/hle/service/vi/system_display_service.h"
#include "core/hle/service/vi/vi_results.h"

namespace Service::VI {

IApplicationDisplayService::IApplicationDisplayService(
    Core::System& system_, Nvnflinger::Nvnflinger& nvnflinger_,
    Nvnflinger::HosBinderDriverServer& hos_binder_driver_server_)
    : ServiceFramework{system_, "IApplicationDisplayService"}, nvnflinger{nvnflinger_},
      hos_binder_driver_server{hos_binder_driver_server_} {

    static const FunctionInfo functions[] = {
        {100, &IApplicationDisplayService::GetRelayService, "GetRelayService"},
        {101, &IApplicationDisplayService::GetSystemDisplayService, "GetSystemDisplayService"},
        {102, &IApplicationDisplayService::GetManagerDisplayService, "GetManagerDisplayService"},
        {103, &IApplicationDisplayService::GetIndirectDisplayTransactionService,
         "GetIndirectDisplayTransactionService"},
        {1000, &IApplicationDisplayService::ListDisplays, "ListDisplays"},
        {1010, &IApplicationDisplayService::OpenDisplay, "OpenDisplay"},
        {1011, &IApplicationDisplayService::OpenDefaultDisplay, "OpenDefaultDisplay"},
        {1020, &IApplicationDisplayService::CloseDisplay, "CloseDisplay"},
        {1101, &IApplicationDisplayService::SetDisplayEnabled, "SetDisplayEnabled"},
        {1102, &IApplicationDisplayService::GetDisplayResolution, "GetDisplayResolution"},
        {2020, &IApplicationDisplayService::OpenLayer, "OpenLayer"},
        {2021, &IApplicationDisplayService::CloseLayer, "CloseLayer"},
        {2030, &IApplicationDisplayService::CreateStrayLayer, "CreateStrayLayer"},
        {2031, &IApplicationDisplayService::DestroyStrayLayer, "DestroyStrayLayer"},
        {2101, &IApplicationDisplayService::SetLayerScalingMode, "SetLayerScalingMode"},
        {2102, &IApplicationDisplayService::ConvertScalingMode, "ConvertScalingMode"},
        {2450, &IApplicationDisplayService::GetIndirectLayerImageMap, "GetIndirectLayerImageMap"},
        {2451, nullptr, "GetIndirectLayerImageCropMap"},
        {2460, &IApplicationDisplayService::GetIndirectLayerImageRequiredMemoryInfo,
         "GetIndirectLayerImageRequiredMemoryInfo"},
        {5202, &IApplicationDisplayService::GetDisplayVsyncEvent, "GetDisplayVsyncEvent"},
        {5203, nullptr, "GetDisplayVsyncEventForDebug"},
    };
    RegisterHandlers(functions);
}

IApplicationDisplayService::~IApplicationDisplayService() {
    for (const auto layer_id : stray_layer_ids) {
        nvnflinger.DestroyLayer(layer_id);
    }
}

void IApplicationDisplayService::GetRelayService(HLERequestContext& ctx) {
    LOG_WARNING(Service_VI, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IHOSBinderDriver>(system, hos_binder_driver_server);
}

void IApplicationDisplayService::GetSystemDisplayService(HLERequestContext& ctx) {
    LOG_WARNING(Service_VI, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ISystemDisplayService>(system, nvnflinger);
}

void IApplicationDisplayService::GetManagerDisplayService(HLERequestContext& ctx) {
    LOG_WARNING(Service_VI, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IManagerDisplayService>(system, nvnflinger);
}

void IApplicationDisplayService::GetIndirectDisplayTransactionService(HLERequestContext& ctx) {
    LOG_WARNING(Service_VI, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IHOSBinderDriver>(system, hos_binder_driver_server);
}

void IApplicationDisplayService::OpenDisplay(HLERequestContext& ctx) {
    LOG_WARNING(Service_VI, "(STUBBED) called");

    IPC::RequestParser rp{ctx};
    const auto name_buf = rp.PopRaw<std::array<char, 0x40>>();

    OpenDisplayImpl(ctx, std::string_view{name_buf.data(), name_buf.size()});
}

void IApplicationDisplayService::OpenDefaultDisplay(HLERequestContext& ctx) {
    LOG_DEBUG(Service_VI, "called");

    OpenDisplayImpl(ctx, "Default");
}

void IApplicationDisplayService::OpenDisplayImpl(HLERequestContext& ctx, std::string_view name) {
    const auto trim_pos = name.find('\0');

    if (trim_pos != std::string_view::npos) {
        name.remove_suffix(name.size() - trim_pos);
    }

    ASSERT_MSG(name == "Default", "Non-default displays aren't supported yet");

    const auto display_id = nvnflinger.OpenDisplay(name);
    if (!display_id) {
        LOG_ERROR(Service_VI, "Display not found! display_name={}", name);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultNotFound);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(*display_id);
}

void IApplicationDisplayService::CloseDisplay(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 display_id = rp.Pop<u64>();

    const Result rc = nvnflinger.CloseDisplay(display_id) ? ResultSuccess : ResultUnknown;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(rc);
}

// This literally does nothing internally in the actual service itself,
// and just returns a successful result code regardless of the input.
void IApplicationDisplayService::SetDisplayEnabled(HLERequestContext& ctx) {
    LOG_DEBUG(Service_VI, "called.");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationDisplayService::GetDisplayResolution(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 display_id = rp.Pop<u64>();

    LOG_DEBUG(Service_VI, "called. display_id=0x{:016X}", display_id);

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);

    // This only returns the fixed values of 1280x720 and makes no distinguishing
    // between docked and undocked dimensions. We take the liberty of applying
    // the resolution scaling factor here.
    rb.Push(static_cast<u64>(DisplayResolution::UndockedWidth));
    rb.Push(static_cast<u64>(DisplayResolution::UndockedHeight));
}

void IApplicationDisplayService::SetLayerScalingMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto scaling_mode = rp.PopEnum<NintendoScaleMode>();
    const u64 unknown = rp.Pop<u64>();

    LOG_DEBUG(Service_VI, "called. scaling_mode=0x{:08X}, unknown=0x{:016X}", scaling_mode,
              unknown);

    IPC::ResponseBuilder rb{ctx, 2};

    if (scaling_mode > NintendoScaleMode::PreserveAspectRatio) {
        LOG_ERROR(Service_VI, "Invalid scaling mode provided.");
        rb.Push(ResultOperationFailed);
        return;
    }

    if (scaling_mode != NintendoScaleMode::ScaleToWindow &&
        scaling_mode != NintendoScaleMode::PreserveAspectRatio) {
        LOG_ERROR(Service_VI, "Unsupported scaling mode supplied.");
        rb.Push(ResultNotSupported);
        return;
    }

    rb.Push(ResultSuccess);
}

void IApplicationDisplayService::ListDisplays(HLERequestContext& ctx) {
    LOG_WARNING(Service_VI, "(STUBBED) called");

    const DisplayInfo display_info;
    ctx.WriteBuffer(&display_info, sizeof(DisplayInfo));
    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(1);
}

void IApplicationDisplayService::OpenLayer(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto name_buf = rp.PopRaw<std::array<u8, 0x40>>();
    const std::string display_name(Common::StringFromBuffer(name_buf));

    const u64 layer_id = rp.Pop<u64>();
    const u64 aruid = rp.Pop<u64>();

    LOG_DEBUG(Service_VI, "called. layer_id=0x{:016X}, aruid=0x{:016X}", layer_id, aruid);

    const auto display_id = nvnflinger.OpenDisplay(display_name);
    if (!display_id) {
        LOG_ERROR(Service_VI, "Layer not found! layer_id={}", layer_id);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultNotFound);
        return;
    }

    const auto buffer_queue_id = nvnflinger.FindBufferQueueId(*display_id, layer_id);
    if (!buffer_queue_id) {
        LOG_ERROR(Service_VI, "Buffer queue id not found! display_id={}", *display_id);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultNotFound);
        return;
    }

    if (!nvnflinger.OpenLayer(layer_id)) {
        LOG_WARNING(Service_VI, "Tried to open layer which was already open");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultOperationFailed);
        return;
    }

    android::OutputParcel parcel;
    parcel.WriteInterface(NativeWindow{*buffer_queue_id});

    const auto buffer_size = ctx.WriteBuffer(parcel.Serialize());

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(buffer_size);
}

void IApplicationDisplayService::CloseLayer(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto layer_id{rp.Pop<u64>()};

    LOG_DEBUG(Service_VI, "called. layer_id=0x{:016X}", layer_id);

    if (!nvnflinger.CloseLayer(layer_id)) {
        LOG_WARNING(Service_VI, "Tried to close layer which was not open");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultOperationFailed);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationDisplayService::CreateStrayLayer(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u32 flags = rp.Pop<u32>();
    rp.Pop<u32>(); // padding
    const u64 display_id = rp.Pop<u64>();

    LOG_DEBUG(Service_VI, "called. flags=0x{:08X}, display_id=0x{:016X}", flags, display_id);

    // TODO(Subv): What's the difference between a Stray and a Managed layer?

    const auto layer_id = nvnflinger.CreateLayer(display_id);
    if (!layer_id) {
        LOG_ERROR(Service_VI, "Layer not found! display_id={}", display_id);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultNotFound);
        return;
    }

    stray_layer_ids.push_back(*layer_id);
    const auto buffer_queue_id = nvnflinger.FindBufferQueueId(display_id, *layer_id);
    if (!buffer_queue_id) {
        LOG_ERROR(Service_VI, "Buffer queue id not found! display_id={}", display_id);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultNotFound);
        return;
    }

    android::OutputParcel parcel;
    parcel.WriteInterface(NativeWindow{*buffer_queue_id});

    const auto buffer_size = ctx.WriteBuffer(parcel.Serialize());

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.Push(*layer_id);
    rb.Push<u64>(buffer_size);
}

void IApplicationDisplayService::DestroyStrayLayer(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 layer_id = rp.Pop<u64>();

    LOG_WARNING(Service_VI, "(STUBBED) called. layer_id=0x{:016X}", layer_id);
    nvnflinger.DestroyLayer(layer_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IApplicationDisplayService::GetDisplayVsyncEvent(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 display_id = rp.Pop<u64>();

    LOG_DEBUG(Service_VI, "called. display_id={}", display_id);

    Kernel::KReadableEvent* vsync_event{};
    const auto result = nvnflinger.FindVsyncEvent(&vsync_event, display_id);
    if (result != ResultSuccess) {
        if (result == ResultNotFound) {
            LOG_ERROR(Service_VI, "Vsync event was not found for display_id={}", display_id);
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }
    if (vsync_event_fetched) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(VI::ResultPermissionDenied);
        return;
    }
    vsync_event_fetched = true;

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(vsync_event);
}

void IApplicationDisplayService::ConvertScalingMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto mode = rp.PopEnum<NintendoScaleMode>();
    LOG_DEBUG(Service_VI, "called mode={}", mode);

    ConvertedScaleMode converted_mode{};
    const auto result = ConvertScalingModeImpl(&converted_mode, mode);

    if (result == ResultSuccess) {
        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(ResultSuccess);
        rb.PushEnum(converted_mode);
    } else {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
    }
}

void IApplicationDisplayService::GetIndirectLayerImageMap(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto width = rp.Pop<s64>();
    const auto height = rp.Pop<s64>();
    const auto indirect_layer_consumer_handle = rp.Pop<u64>();
    const auto applet_resource_user_id = rp.Pop<u64>();

    LOG_WARNING(Service_VI,
                "(STUBBED) called, width={}, height={}, indirect_layer_consumer_handle={}, "
                "applet_resource_user_id={}",
                width, height, indirect_layer_consumer_handle, applet_resource_user_id);

    std::vector<u8> out_buffer(0x46);
    ctx.WriteBuffer(out_buffer);

    // TODO: Figure out what these are

    constexpr s64 unknown_result_1 = 0;
    constexpr s64 unknown_result_2 = 0;

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(unknown_result_1);
    rb.Push(unknown_result_2);
    rb.Push(ResultSuccess);
}

void IApplicationDisplayService::GetIndirectLayerImageRequiredMemoryInfo(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto width = rp.Pop<u64>();
    const auto height = rp.Pop<u64>();
    LOG_DEBUG(Service_VI, "called width={}, height={}", width, height);

    constexpr u64 base_size = 0x20000;
    constexpr u64 alignment = 0x1000;
    const auto texture_size = width * height * 4;
    const auto out_size = (texture_size + base_size - 1) / base_size * base_size;

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.Push(out_size);
    rb.Push(alignment);
}

Result IApplicationDisplayService::ConvertScalingModeImpl(ConvertedScaleMode* out_scaling_mode,
                                                          NintendoScaleMode mode) {
    switch (mode) {
    case NintendoScaleMode::None:
        *out_scaling_mode = ConvertedScaleMode::None;
        return ResultSuccess;
    case NintendoScaleMode::Freeze:
        *out_scaling_mode = ConvertedScaleMode::Freeze;
        return ResultSuccess;
    case NintendoScaleMode::ScaleToWindow:
        *out_scaling_mode = ConvertedScaleMode::ScaleToWindow;
        return ResultSuccess;
    case NintendoScaleMode::ScaleAndCrop:
        *out_scaling_mode = ConvertedScaleMode::ScaleAndCrop;
        return ResultSuccess;
    case NintendoScaleMode::PreserveAspectRatio:
        *out_scaling_mode = ConvertedScaleMode::PreserveAspectRatio;
        return ResultSuccess;
    default:
        LOG_ERROR(Service_VI, "Invalid scaling mode specified, mode={}", mode);
        return ResultOperationFailed;
    }
}

} // namespace Service::VI
