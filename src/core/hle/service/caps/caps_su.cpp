// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/caps/caps_manager.h"
#include "core/hle/service/caps/caps_su.h"
#include "core/hle/service/caps/caps_types.h"
#include "core/hle/service/ipc_helpers.h"
#include "video_core/renderer_base.h"

namespace Service::Capture {

IScreenShotApplicationService::IScreenShotApplicationService(
    Core::System& system_, std::shared_ptr<AlbumManager> album_manager)
    : ServiceFramework{system_, "caps:su"}, manager{album_manager} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {32, &IScreenShotApplicationService::SetShimLibraryVersion, "SetShimLibraryVersion"},
        {201, nullptr, "SaveScreenShot"},
        {203, &IScreenShotApplicationService::SaveScreenShotEx0, "SaveScreenShotEx0"},
        {205, &IScreenShotApplicationService::SaveScreenShotEx1, "SaveScreenShotEx1"},
        {210, nullptr, "SaveScreenShotEx2"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IScreenShotApplicationService::~IScreenShotApplicationService() = default;

void IScreenShotApplicationService::SetShimLibraryVersion(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto library_version{rp.Pop<u64>()};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_WARNING(Service_Capture, "(STUBBED) called. library_version={}, applet_resource_user_id={}",
                library_version, applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IScreenShotApplicationService::SaveScreenShotEx0(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        ScreenShotAttribute attribute{};
        AlbumReportOption report_option{};
        INSERT_PADDING_BYTES(0x4);
        u64 applet_resource_user_id{};
    };
    static_assert(sizeof(Parameters) == 0x50, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};
    const auto image_data_buffer = ctx.ReadBuffer();

    LOG_INFO(Service_Capture,
             "called, report_option={}, image_data_buffer_size={}, applet_resource_user_id={}",
             parameters.report_option, image_data_buffer.size(),
             parameters.applet_resource_user_id);

    ApplicationAlbumEntry entry{};
    manager->FlipVerticallyOnWrite(false);
    const auto result =
        manager->SaveScreenShot(entry, parameters.attribute, parameters.report_option,
                                image_data_buffer, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 10};
    rb.Push(result);
    rb.PushRaw(entry);
}

void IScreenShotApplicationService::SaveScreenShotEx1(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        ScreenShotAttribute attribute{};
        AlbumReportOption report_option{};
        INSERT_PADDING_BYTES(0x4);
        u64 applet_resource_user_id{};
    };
    static_assert(sizeof(Parameters) == 0x50, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};
    const auto app_data_buffer = ctx.ReadBuffer(0);
    const auto image_data_buffer = ctx.ReadBuffer(1);

    LOG_INFO(Service_Capture,
             "called, report_option={}, image_data_buffer_size={}, applet_resource_user_id={}",
             parameters.report_option, image_data_buffer.size(),
             parameters.applet_resource_user_id);

    ApplicationAlbumEntry entry{};
    ApplicationData app_data{};
    std::memcpy(&app_data, app_data_buffer.data(), sizeof(ApplicationData));
    manager->FlipVerticallyOnWrite(false);
    const auto result =
        manager->SaveScreenShot(entry, parameters.attribute, parameters.report_option, app_data,
                                image_data_buffer, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 10};
    rb.Push(result);
    rb.PushRaw(entry);
}

void IScreenShotApplicationService::CaptureAndSaveScreenshot(AlbumReportOption report_option) {
    auto& renderer = system.Renderer();
    Layout::FramebufferLayout layout =
        Layout::DefaultFrameLayout(screenshot_width, screenshot_height);

    const Capture::ScreenShotAttribute attribute{
        .unknown_0{},
        .orientation = Capture::AlbumImageOrientation::None,
        .unknown_1{},
        .unknown_2{},
    };

    renderer.RequestScreenshot(
        image_data.data(),
        [attribute, report_option, this](bool invert_y) {
            // Convert from BGRA to RGBA
            for (std::size_t i = 0; i < image_data.size(); i += bytes_per_pixel) {
                const u8 temp = image_data[i];
                image_data[i] = image_data[i + 2];
                image_data[i + 2] = temp;
            }

            Capture::ApplicationAlbumEntry entry{};
            manager->FlipVerticallyOnWrite(invert_y);
            manager->SaveScreenShot(entry, attribute, report_option, image_data, {});
        },
        layout);
}

} // namespace Service::Capture
