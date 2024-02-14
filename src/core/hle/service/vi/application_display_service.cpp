// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/nvnflinger/nvnflinger.h"
#include "core/hle/service/nvnflinger/parcel.h"
#include "core/hle/service/vi/application_display_service.h"
#include "core/hle/service/vi/hos_binder_driver.h"
#include "core/hle/service/vi/manager_display_service.h"
#include "core/hle/service/vi/system_display_service.h"
#include "core/hle/service/vi/vi_results.h"

namespace Service::VI {

IApplicationDisplayService::IApplicationDisplayService(
    Core::System& system_, Nvnflinger::Nvnflinger& nvnflinger,
    Nvnflinger::HosBinderDriverServer& hos_binder_driver_server)
    : ServiceFramework{system_, "IApplicationDisplayService"}, m_nvnflinger{nvnflinger},
      m_hos_binder_driver_server{hos_binder_driver_server} {

    // clang-format off
    static const FunctionInfo functions[] = {
        {100, C<&IApplicationDisplayService::GetRelayService>, "GetRelayService"},
        {101, C<&IApplicationDisplayService::GetSystemDisplayService>, "GetSystemDisplayService"},
        {102, C<&IApplicationDisplayService::GetManagerDisplayService>, "GetManagerDisplayService"},
        {103, C<&IApplicationDisplayService::GetIndirectDisplayTransactionService>, "GetIndirectDisplayTransactionService"},
        {1000, C<&IApplicationDisplayService::ListDisplays>, "ListDisplays"},
        {1010, C<&IApplicationDisplayService::OpenDisplay>, "OpenDisplay"},
        {1011, C<&IApplicationDisplayService::OpenDefaultDisplay>, "OpenDefaultDisplay"},
        {1020, C<&IApplicationDisplayService::CloseDisplay>, "CloseDisplay"},
        {1101, C<&IApplicationDisplayService::SetDisplayEnabled>, "SetDisplayEnabled"},
        {1102, C<&IApplicationDisplayService::GetDisplayResolution>, "GetDisplayResolution"},
        {2020, C<&IApplicationDisplayService::OpenLayer>, "OpenLayer"},
        {2021, C<&IApplicationDisplayService::CloseLayer>, "CloseLayer"},
        {2030, C<&IApplicationDisplayService::CreateStrayLayer>, "CreateStrayLayer"},
        {2031, C<&IApplicationDisplayService::DestroyStrayLayer>, "DestroyStrayLayer"},
        {2101, C<&IApplicationDisplayService::SetLayerScalingMode>, "SetLayerScalingMode"},
        {2102, C<&IApplicationDisplayService::ConvertScalingMode>, "ConvertScalingMode"},
        {2450, C<&IApplicationDisplayService::GetIndirectLayerImageMap>, "GetIndirectLayerImageMap"},
        {2451, nullptr, "GetIndirectLayerImageCropMap"},
        {2460, C<&IApplicationDisplayService::GetIndirectLayerImageRequiredMemoryInfo>, "GetIndirectLayerImageRequiredMemoryInfo"},
        {5202, C<&IApplicationDisplayService::GetDisplayVsyncEvent>, "GetDisplayVsyncEvent"},
        {5203, nullptr, "GetDisplayVsyncEventForDebug"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IApplicationDisplayService::~IApplicationDisplayService() {
    for (const auto layer_id : m_stray_layer_ids) {
        m_nvnflinger.DestroyLayer(layer_id);
    }
}

Result IApplicationDisplayService::GetRelayService(
    Out<SharedPointer<IHOSBinderDriver>> out_relay_service) {
    LOG_WARNING(Service_VI, "(STUBBED) called");
    *out_relay_service = std::make_shared<IHOSBinderDriver>(system, m_hos_binder_driver_server);
    R_SUCCEED();
}

Result IApplicationDisplayService::GetSystemDisplayService(
    Out<SharedPointer<ISystemDisplayService>> out_system_display_service) {
    LOG_WARNING(Service_VI, "(STUBBED) called");
    *out_system_display_service = std::make_shared<ISystemDisplayService>(system, m_nvnflinger);
    R_SUCCEED();
}

Result IApplicationDisplayService::GetManagerDisplayService(
    Out<SharedPointer<IManagerDisplayService>> out_manager_display_service) {
    LOG_WARNING(Service_VI, "(STUBBED) called");
    *out_manager_display_service = std::make_shared<IManagerDisplayService>(system, m_nvnflinger);
    R_SUCCEED();
}

Result IApplicationDisplayService::GetIndirectDisplayTransactionService(
    Out<SharedPointer<IHOSBinderDriver>> out_indirect_display_transaction_service) {
    LOG_WARNING(Service_VI, "(STUBBED) called");
    *out_indirect_display_transaction_service =
        std::make_shared<IHOSBinderDriver>(system, m_hos_binder_driver_server);
    R_SUCCEED();
}

Result IApplicationDisplayService::OpenDisplay(Out<u64> out_display_id, DisplayName display_name) {
    LOG_WARNING(Service_VI, "(STUBBED) called");

    display_name[display_name.size() - 1] = '\0';
    ASSERT_MSG(strcmp(display_name.data(), "Default") == 0,
               "Non-default displays aren't supported yet");

    const auto display_id = m_nvnflinger.OpenDisplay(display_name.data());
    if (!display_id) {
        LOG_ERROR(Service_VI, "Display not found! display_name={}", display_name.data());
        R_THROW(VI::ResultNotFound);
    }

    *out_display_id = *display_id;
    R_SUCCEED();
}

Result IApplicationDisplayService::OpenDefaultDisplay(Out<u64> out_display_id) {
    LOG_DEBUG(Service_VI, "called");
    R_RETURN(this->OpenDisplay(out_display_id, DisplayName{"Default"}));
}

Result IApplicationDisplayService::CloseDisplay(u64 display_id) {
    LOG_DEBUG(Service_VI, "called");
    R_SUCCEED_IF(m_nvnflinger.CloseDisplay(display_id));
    R_THROW(ResultUnknown);
}

Result IApplicationDisplayService::SetDisplayEnabled(u32 state, u64 display_id) {
    LOG_DEBUG(Service_VI, "called");

    // This literally does nothing internally in the actual service itself,
    // and just returns a successful result code regardless of the input.
    R_SUCCEED();
}

Result IApplicationDisplayService::GetDisplayResolution(Out<s64> out_width, Out<s64> out_height,
                                                        u64 display_id) {
    LOG_DEBUG(Service_VI, "called. display_id={}", display_id);

    // This only returns the fixed values of 1280x720 and makes no distinguishing
    // between docked and undocked dimensions.
    *out_width = static_cast<s64>(DisplayResolution::UndockedWidth);
    *out_height = static_cast<s64>(DisplayResolution::UndockedHeight);
    R_SUCCEED();
}

Result IApplicationDisplayService::SetLayerScalingMode(NintendoScaleMode scale_mode, u64 layer_id) {
    LOG_DEBUG(Service_VI, "called. scale_mode={}, unknown=0x{:016X}", scale_mode, layer_id);

    if (scale_mode > NintendoScaleMode::PreserveAspectRatio) {
        LOG_ERROR(Service_VI, "Invalid scaling mode provided.");
        R_THROW(VI::ResultOperationFailed);
    }

    if (scale_mode != NintendoScaleMode::ScaleToWindow &&
        scale_mode != NintendoScaleMode::PreserveAspectRatio) {
        LOG_ERROR(Service_VI, "Unsupported scaling mode supplied.");
        R_THROW(VI::ResultNotSupported);
    }

    R_SUCCEED();
}

Result IApplicationDisplayService::ListDisplays(
    Out<u64> out_count, OutArray<DisplayInfo, BufferAttr_HipcMapAlias> out_displays) {
    LOG_WARNING(Service_VI, "(STUBBED) called");

    if (out_displays.size() > 0) {
        out_displays[0] = DisplayInfo{};
        *out_count = 1;
    } else {
        *out_count = 0;
    }

    R_SUCCEED();
}

Result IApplicationDisplayService::OpenLayer(Out<u64> out_size,
                                             OutBuffer<BufferAttr_HipcMapAlias> out_native_window,
                                             DisplayName display_name, u64 layer_id,
                                             ClientAppletResourceUserId aruid) {
    display_name[display_name.size() - 1] = '\0';

    LOG_DEBUG(Service_VI, "called. layer_id={}, aruid={:#x}", layer_id, aruid.pid);

    const auto display_id = m_nvnflinger.OpenDisplay(display_name.data());
    if (!display_id) {
        LOG_ERROR(Service_VI, "Layer not found! layer_id={}", layer_id);
        R_THROW(VI::ResultNotFound);
    }

    const auto buffer_queue_id = m_nvnflinger.FindBufferQueueId(*display_id, layer_id);
    if (!buffer_queue_id) {
        LOG_ERROR(Service_VI, "Buffer queue id not found! display_id={}", *display_id);
        R_THROW(VI::ResultNotFound);
    }

    if (!m_nvnflinger.OpenLayer(layer_id)) {
        LOG_WARNING(Service_VI, "Tried to open layer which was already open");
        R_THROW(VI::ResultOperationFailed);
    }

    android::OutputParcel parcel;
    parcel.WriteInterface(NativeWindow{*buffer_queue_id});

    const auto buffer = parcel.Serialize();
    std::memcpy(out_native_window.data(), buffer.data(),
                std::min(out_native_window.size(), buffer.size()));
    *out_size = buffer.size();

    R_SUCCEED();
}

Result IApplicationDisplayService::CloseLayer(u64 layer_id) {
    LOG_DEBUG(Service_VI, "called. layer_id={}", layer_id);

    if (!m_nvnflinger.CloseLayer(layer_id)) {
        LOG_WARNING(Service_VI, "Tried to close layer which was not open");
        R_THROW(VI::ResultOperationFailed);
    }

    R_SUCCEED();
}

Result IApplicationDisplayService::CreateStrayLayer(
    Out<u64> out_layer_id, Out<u64> out_size, OutBuffer<BufferAttr_HipcMapAlias> out_native_window,
    u32 flags, u64 display_id) {
    LOG_DEBUG(Service_VI, "called. flags={}, display_id={}", flags, display_id);

    const auto layer_id = m_nvnflinger.CreateLayer(display_id);
    if (!layer_id) {
        LOG_ERROR(Service_VI, "Layer not found! display_id={}", display_id);
        R_THROW(VI::ResultNotFound);
    }

    m_stray_layer_ids.push_back(*layer_id);
    const auto buffer_queue_id = m_nvnflinger.FindBufferQueueId(display_id, *layer_id);
    if (!buffer_queue_id) {
        LOG_ERROR(Service_VI, "Buffer queue id not found! display_id={}", display_id);
        R_THROW(VI::ResultNotFound);
    }

    android::OutputParcel parcel;
    parcel.WriteInterface(NativeWindow{*buffer_queue_id});

    const auto buffer = parcel.Serialize();
    std::memcpy(out_native_window.data(), buffer.data(),
                std::min(out_native_window.size(), buffer.size()));

    *out_layer_id = *layer_id;
    *out_size = buffer.size();

    R_SUCCEED();
}

Result IApplicationDisplayService::DestroyStrayLayer(u64 layer_id) {
    LOG_WARNING(Service_VI, "(STUBBED) called. layer_id={}", layer_id);
    m_nvnflinger.DestroyLayer(layer_id);
    R_SUCCEED();
}

Result IApplicationDisplayService::GetDisplayVsyncEvent(
    OutCopyHandle<Kernel::KReadableEvent> out_vsync_event, u64 display_id) {
    LOG_DEBUG(Service_VI, "called. display_id={}", display_id);

    const auto result = m_nvnflinger.FindVsyncEvent(out_vsync_event, display_id);
    if (result != ResultSuccess) {
        if (result == ResultNotFound) {
            LOG_ERROR(Service_VI, "Vsync event was not found for display_id={}", display_id);
        }

        R_THROW(result);
    }

    R_UNLESS(!m_vsync_event_fetched, VI::ResultPermissionDenied);
    m_vsync_event_fetched = true;

    R_SUCCEED();
}

Result IApplicationDisplayService::ConvertScalingMode(Out<ConvertedScaleMode> out_scaling_mode,
                                                      NintendoScaleMode mode) {
    LOG_DEBUG(Service_VI, "called mode={}", mode);

    switch (mode) {
    case NintendoScaleMode::None:
        *out_scaling_mode = ConvertedScaleMode::None;
        R_SUCCEED();
    case NintendoScaleMode::Freeze:
        *out_scaling_mode = ConvertedScaleMode::Freeze;
        R_SUCCEED();
    case NintendoScaleMode::ScaleToWindow:
        *out_scaling_mode = ConvertedScaleMode::ScaleToWindow;
        R_SUCCEED();
    case NintendoScaleMode::ScaleAndCrop:
        *out_scaling_mode = ConvertedScaleMode::ScaleAndCrop;
        R_SUCCEED();
    case NintendoScaleMode::PreserveAspectRatio:
        *out_scaling_mode = ConvertedScaleMode::PreserveAspectRatio;
        R_SUCCEED();
    default:
        LOG_ERROR(Service_VI, "Invalid scaling mode specified, mode={}", mode);
        R_THROW(VI::ResultOperationFailed);
    }
}

Result IApplicationDisplayService::GetIndirectLayerImageMap(
    Out<u64> out_size, Out<u64> out_stride,
    OutBuffer<BufferAttr_HipcMapTransferAllowsNonSecure | BufferAttr_HipcMapAlias> out_buffer,
    s64 width, s64 height, u64 indirect_layer_consumer_handle, ClientAppletResourceUserId aruid) {
    LOG_WARNING(
        Service_VI,
        "(STUBBED) called, width={}, height={}, indirect_layer_consumer_handle={}, aruid={:#x}",
        width, height, indirect_layer_consumer_handle, aruid.pid);
    *out_size = 0;
    *out_stride = 0;
    R_SUCCEED();
}

Result IApplicationDisplayService::GetIndirectLayerImageRequiredMemoryInfo(Out<s64> out_size,
                                                                           Out<s64> out_alignment,
                                                                           s64 width, s64 height) {
    LOG_DEBUG(Service_VI, "called width={}, height={}", width, height);

    constexpr u64 base_size = 0x20000;
    const auto texture_size = width * height * 4;

    *out_alignment = 0x1000;
    *out_size = (texture_size + base_size - 1) / base_size * base_size;

    R_SUCCEED();
}

} // namespace Service::VI
