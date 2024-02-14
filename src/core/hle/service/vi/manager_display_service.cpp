// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/nvnflinger/nvnflinger.h"
#include "core/hle/service/vi/manager_display_service.h"
#include "core/hle/service/vi/vi_results.h"

namespace Service::VI {

IManagerDisplayService::IManagerDisplayService(Core::System& system_,
                                               Nvnflinger::Nvnflinger& nvnflinger_)
    : ServiceFramework{system_, "IManagerDisplayService"}, nvnflinger{nvnflinger_} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {200, nullptr, "AllocateProcessHeapBlock"},
        {201, nullptr, "FreeProcessHeapBlock"},
        {1020, &IManagerDisplayService::CloseDisplay, "CloseDisplay"},
        {1102, nullptr, "GetDisplayResolution"},
        {2010, &IManagerDisplayService::CreateManagedLayer, "CreateManagedLayer"},
        {2011, nullptr, "DestroyManagedLayer"},
        {2012, nullptr, "CreateStrayLayer"},
        {2050, nullptr, "CreateIndirectLayer"},
        {2051, nullptr, "DestroyIndirectLayer"},
        {2052, nullptr, "CreateIndirectProducerEndPoint"},
        {2053, nullptr, "DestroyIndirectProducerEndPoint"},
        {2054, nullptr, "CreateIndirectConsumerEndPoint"},
        {2055, nullptr, "DestroyIndirectConsumerEndPoint"},
        {2060, nullptr, "CreateWatermarkCompositor"},
        {2062, nullptr, "SetWatermarkText"},
        {2063, nullptr, "SetWatermarkLayerStacks"},
        {2300, nullptr, "AcquireLayerTexturePresentingEvent"},
        {2301, nullptr, "ReleaseLayerTexturePresentingEvent"},
        {2302, nullptr, "GetDisplayHotplugEvent"},
        {2303, nullptr, "GetDisplayModeChangedEvent"},
        {2402, nullptr, "GetDisplayHotplugState"},
        {2501, nullptr, "GetCompositorErrorInfo"},
        {2601, nullptr, "GetDisplayErrorEvent"},
        {2701, nullptr, "GetDisplayFatalErrorEvent"},
        {4201, nullptr, "SetDisplayAlpha"},
        {4203, nullptr, "SetDisplayLayerStack"},
        {4205, nullptr, "SetDisplayPowerState"},
        {4206, nullptr, "SetDefaultDisplay"},
        {4207, nullptr, "ResetDisplayPanel"},
        {4208, nullptr, "SetDisplayFatalErrorEnabled"},
        {4209, nullptr, "IsDisplayPanelOn"},
        {4300, nullptr, "GetInternalPanelId"},
        {6000, &IManagerDisplayService::AddToLayerStack, "AddToLayerStack"},
        {6001, nullptr, "RemoveFromLayerStack"},
        {6002, &IManagerDisplayService::SetLayerVisibility, "SetLayerVisibility"},
        {6003, nullptr, "SetLayerConfig"},
        {6004, nullptr, "AttachLayerPresentationTracer"},
        {6005, nullptr, "DetachLayerPresentationTracer"},
        {6006, nullptr, "StartLayerPresentationRecording"},
        {6007, nullptr, "StopLayerPresentationRecording"},
        {6008, nullptr, "StartLayerPresentationFenceWait"},
        {6009, nullptr, "StopLayerPresentationFenceWait"},
        {6010, nullptr, "GetLayerPresentationAllFencesExpiredEvent"},
        {6011, nullptr, "EnableLayerAutoClearTransitionBuffer"},
        {6012, nullptr, "DisableLayerAutoClearTransitionBuffer"},
        {6013, nullptr, "SetLayerOpacity"},
        {6014, nullptr, "AttachLayerWatermarkCompositor"},
        {6015, nullptr, "DetachLayerWatermarkCompositor"},
        {7000, nullptr, "SetContentVisibility"},
        {8000, nullptr, "SetConductorLayer"},
        {8001, nullptr, "SetTimestampTracking"},
        {8100, nullptr, "SetIndirectProducerFlipOffset"},
        {8200, nullptr, "CreateSharedBufferStaticStorage"},
        {8201, nullptr, "CreateSharedBufferTransferMemory"},
        {8202, nullptr, "DestroySharedBuffer"},
        {8203, nullptr, "BindSharedLowLevelLayerToManagedLayer"},
        {8204, nullptr, "BindSharedLowLevelLayerToIndirectLayer"},
        {8207, nullptr, "UnbindSharedLowLevelLayer"},
        {8208, nullptr, "ConnectSharedLowLevelLayerToSharedBuffer"},
        {8209, nullptr, "DisconnectSharedLowLevelLayerFromSharedBuffer"},
        {8210, nullptr, "CreateSharedLayer"},
        {8211, nullptr, "DestroySharedLayer"},
        {8216, nullptr, "AttachSharedLayerToLowLevelLayer"},
        {8217, nullptr, "ForceDetachSharedLayerFromLowLevelLayer"},
        {8218, nullptr, "StartDetachSharedLayerFromLowLevelLayer"},
        {8219, nullptr, "FinishDetachSharedLayerFromLowLevelLayer"},
        {8220, nullptr, "GetSharedLayerDetachReadyEvent"},
        {8221, nullptr, "GetSharedLowLevelLayerSynchronizedEvent"},
        {8222, nullptr, "CheckSharedLowLevelLayerSynchronized"},
        {8223, nullptr, "RegisterSharedBufferImporterAruid"},
        {8224, nullptr, "UnregisterSharedBufferImporterAruid"},
        {8227, nullptr, "CreateSharedBufferProcessHeap"},
        {8228, nullptr, "GetSharedLayerLayerStacks"},
        {8229, nullptr, "SetSharedLayerLayerStacks"},
        {8291, nullptr, "PresentDetachedSharedFrameBufferToLowLevelLayer"},
        {8292, nullptr, "FillDetachedSharedFrameBufferColor"},
        {8293, nullptr, "GetDetachedSharedFrameBufferImage"},
        {8294, nullptr, "SetDetachedSharedFrameBufferImage"},
        {8295, nullptr, "CopyDetachedSharedFrameBufferImage"},
        {8296, nullptr, "SetDetachedSharedFrameBufferSubImage"},
        {8297, nullptr, "GetSharedFrameBufferContentParameter"},
        {8298, nullptr, "ExpandStartupLogoOnSharedFrameBuffer"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IManagerDisplayService::~IManagerDisplayService() = default;

void IManagerDisplayService::CloseDisplay(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 display = rp.Pop<u64>();

    const Result rc = nvnflinger.CloseDisplay(display) ? ResultSuccess : ResultUnknown;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(rc);
}

void IManagerDisplayService::CreateManagedLayer(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u32 unknown = rp.Pop<u32>();
    rp.Skip(1, false);
    const u64 display = rp.Pop<u64>();
    const u64 aruid = rp.Pop<u64>();

    LOG_WARNING(Service_VI,
                "(STUBBED) called. unknown=0x{:08X}, display=0x{:016X}, aruid=0x{:016X}", unknown,
                display, aruid);

    const auto layer_id = nvnflinger.CreateLayer(display);
    if (!layer_id) {
        LOG_ERROR(Service_VI, "Layer not found! display=0x{:016X}", display);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultNotFound);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(*layer_id);
}

void IManagerDisplayService::AddToLayerStack(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u32 stack = rp.Pop<u32>();
    const u64 layer_id = rp.Pop<u64>();

    LOG_WARNING(Service_VI, "(STUBBED) called. stack=0x{:08X}, layer_id=0x{:016X}", stack,
                layer_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IManagerDisplayService::SetLayerVisibility(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 layer_id = rp.Pop<u64>();
    const bool visibility = rp.Pop<bool>();

    LOG_WARNING(Service_VI, "(STUBBED) called, layer_id=0x{:X}, visibility={}", layer_id,
                visibility);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

} // namespace Service::VI
