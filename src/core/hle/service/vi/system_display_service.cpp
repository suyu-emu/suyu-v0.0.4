// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/settings.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/nvnflinger/fb_share_buffer_manager.h"
#include "core/hle/service/vi/system_display_service.h"
#include "core/hle/service/vi/vi_types.h"

namespace Service::VI {

ISystemDisplayService::ISystemDisplayService(Core::System& system_,
                                             Nvnflinger::Nvnflinger& nvnflinger_)
    : ServiceFramework{system_, "ISystemDisplayService"}, nvnflinger{nvnflinger_} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {1200, nullptr, "GetZOrderCountMin"},
        {1202, nullptr, "GetZOrderCountMax"},
        {1203, nullptr, "GetDisplayLogicalResolution"},
        {1204, nullptr, "SetDisplayMagnification"},
        {2201, nullptr, "SetLayerPosition"},
        {2203, nullptr, "SetLayerSize"},
        {2204, nullptr, "GetLayerZ"},
        {2205, &ISystemDisplayService::SetLayerZ, "SetLayerZ"},
        {2207, &ISystemDisplayService::SetLayerVisibility, "SetLayerVisibility"},
        {2209, nullptr, "SetLayerAlpha"},
        {2210, nullptr, "SetLayerPositionAndSize"},
        {2312, nullptr, "CreateStrayLayer"},
        {2400, nullptr, "OpenIndirectLayer"},
        {2401, nullptr, "CloseIndirectLayer"},
        {2402, nullptr, "FlipIndirectLayer"},
        {3000, nullptr, "ListDisplayModes"},
        {3001, nullptr, "ListDisplayRgbRanges"},
        {3002, nullptr, "ListDisplayContentTypes"},
        {3200, &ISystemDisplayService::GetDisplayMode, "GetDisplayMode"},
        {3201, nullptr, "SetDisplayMode"},
        {3202, nullptr, "GetDisplayUnderscan"},
        {3203, nullptr, "SetDisplayUnderscan"},
        {3204, nullptr, "GetDisplayContentType"},
        {3205, nullptr, "SetDisplayContentType"},
        {3206, nullptr, "GetDisplayRgbRange"},
        {3207, nullptr, "SetDisplayRgbRange"},
        {3208, nullptr, "GetDisplayCmuMode"},
        {3209, nullptr, "SetDisplayCmuMode"},
        {3210, nullptr, "GetDisplayContrastRatio"},
        {3211, nullptr, "SetDisplayContrastRatio"},
        {3214, nullptr, "GetDisplayGamma"},
        {3215, nullptr, "SetDisplayGamma"},
        {3216, nullptr, "GetDisplayCmuLuma"},
        {3217, nullptr, "SetDisplayCmuLuma"},
        {3218, nullptr, "SetDisplayCrcMode"},
        {6013, nullptr, "GetLayerPresentationSubmissionTimestamps"},
        {8225, &ISystemDisplayService::GetSharedBufferMemoryHandleId, "GetSharedBufferMemoryHandleId"},
        {8250, &ISystemDisplayService::OpenSharedLayer, "OpenSharedLayer"},
        {8251, nullptr, "CloseSharedLayer"},
        {8252, &ISystemDisplayService::ConnectSharedLayer, "ConnectSharedLayer"},
        {8253, nullptr, "DisconnectSharedLayer"},
        {8254, &ISystemDisplayService::AcquireSharedFrameBuffer, "AcquireSharedFrameBuffer"},
        {8255, &ISystemDisplayService::PresentSharedFrameBuffer, "PresentSharedFrameBuffer"},
        {8256, &ISystemDisplayService::GetSharedFrameBufferAcquirableEvent, "GetSharedFrameBufferAcquirableEvent"},
        {8257, nullptr, "FillSharedFrameBufferColor"},
        {8258, nullptr, "CancelSharedFrameBuffer"},
        {9000, nullptr, "GetDp2hdmiController"},
    };
    // clang-format on
    RegisterHandlers(functions);
}

ISystemDisplayService::~ISystemDisplayService() = default;

void ISystemDisplayService::GetSharedBufferMemoryHandleId(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 buffer_id = rp.PopRaw<u64>();
    const u64 aruid = ctx.GetPID();

    LOG_INFO(Service_VI, "called. buffer_id={:#x}, aruid={:#x}", buffer_id, aruid);

    struct OutputParameters {
        s32 nvmap_handle;
        u64 size;
    };

    OutputParameters out{};
    Nvnflinger::SharedMemoryPoolLayout layout{};
    const auto result = nvnflinger.GetSystemBufferManager().GetSharedBufferMemoryHandleId(
        &out.size, &out.nvmap_handle, &layout, buffer_id, aruid);

    ctx.WriteBuffer(&layout, sizeof(layout));

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(result);
    rb.PushRaw(out);
}

void ISystemDisplayService::OpenSharedLayer(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 layer_id = rp.PopRaw<u64>();

    LOG_INFO(Service_VI, "(STUBBED) called. layer_id={:#x}", layer_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemDisplayService::ConnectSharedLayer(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 layer_id = rp.PopRaw<u64>();

    LOG_INFO(Service_VI, "(STUBBED) called. layer_id={:#x}", layer_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemDisplayService::GetSharedFrameBufferAcquirableEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_VI, "called");

    IPC::RequestParser rp{ctx};
    const u64 layer_id = rp.PopRaw<u64>();

    Kernel::KReadableEvent* event{};
    const auto result =
        nvnflinger.GetSystemBufferManager().GetSharedFrameBufferAcquirableEvent(&event, layer_id);

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(result);
    rb.PushCopyObjects(event);
}

void ISystemDisplayService::AcquireSharedFrameBuffer(HLERequestContext& ctx) {
    LOG_DEBUG(Service_VI, "called");

    IPC::RequestParser rp{ctx};
    const u64 layer_id = rp.PopRaw<u64>();

    struct OutputParameters {
        android::Fence fence;
        std::array<s32, 4> slots;
        s64 target_slot;
    };
    static_assert(sizeof(OutputParameters) == 0x40, "OutputParameters has wrong size");

    OutputParameters out{};
    const auto result = nvnflinger.GetSystemBufferManager().AcquireSharedFrameBuffer(
        &out.fence, out.slots, &out.target_slot, layer_id);

    IPC::ResponseBuilder rb{ctx, 18};
    rb.Push(result);
    rb.PushRaw(out);
}

void ISystemDisplayService::PresentSharedFrameBuffer(HLERequestContext& ctx) {
    LOG_DEBUG(Service_VI, "called");

    struct InputParameters {
        android::Fence fence;
        Common::Rectangle<s32> crop_region;
        u32 window_transform;
        s32 swap_interval;
        u64 layer_id;
        s64 surface_id;
    };
    static_assert(sizeof(InputParameters) == 0x50, "InputParameters has wrong size");

    IPC::RequestParser rp{ctx};
    auto input = rp.PopRaw<InputParameters>();

    const auto result = nvnflinger.GetSystemBufferManager().PresentSharedFrameBuffer(
        input.fence, input.crop_region, input.window_transform, input.swap_interval, input.layer_id,
        input.surface_id);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void ISystemDisplayService::SetLayerZ(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 layer_id = rp.Pop<u64>();
    const u64 z_value = rp.Pop<u64>();

    LOG_WARNING(Service_VI, "(STUBBED) called. layer_id=0x{:016X}, z_value=0x{:016X}", layer_id,
                z_value);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

// This function currently does nothing but return a success error code in
// the vi library itself, so do the same thing, but log out the passed in values.
void ISystemDisplayService::SetLayerVisibility(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 layer_id = rp.Pop<u64>();
    const bool visibility = rp.Pop<bool>();

    LOG_DEBUG(Service_VI, "called, layer_id=0x{:08X}, visibility={}", layer_id, visibility);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ISystemDisplayService::GetDisplayMode(HLERequestContext& ctx) {
    LOG_WARNING(Service_VI, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);

    if (Settings::IsDockedMode()) {
        rb.Push(static_cast<u32>(DisplayResolution::DockedWidth));
        rb.Push(static_cast<u32>(DisplayResolution::DockedHeight));
    } else {
        rb.Push(static_cast<u32>(DisplayResolution::UndockedWidth));
        rb.Push(static_cast<u32>(DisplayResolution::UndockedHeight));
    }

    rb.PushRaw<float>(60.0f); // This wouldn't seem to be correct for 30 fps games.
    rb.Push<u32>(0);
}

} // namespace Service::VI
