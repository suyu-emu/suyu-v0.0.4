// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/hle/service/caps/caps_manager.h"
#include "core/hle/service/caps/caps_types.h"
#include "core/hle/service/ipc_helpers.h"

#include "core/hle/service/caps/caps_ss.h"

namespace Service::Capture {

IScreenShotService::IScreenShotService(Core::System& system_,
                                       std::shared_ptr<AlbumManager> album_manager)
    : ServiceFramework{system_, "caps:ss"}, manager{album_manager} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {201, nullptr, "SaveScreenShot"},
        {202, nullptr, "SaveEditedScreenShot"},
        {203, &IScreenShotService::SaveScreenShotEx0, "SaveScreenShotEx0"},
        {204, nullptr, "SaveEditedScreenShotEx0"},
        {206, &IScreenShotService::SaveEditedScreenShotEx1, "SaveEditedScreenShotEx1"},
        {208, nullptr, "SaveScreenShotOfMovieEx1"},
        {1000, nullptr, "Unknown1000"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IScreenShotService::~IScreenShotService() = default;

void IScreenShotService::SaveScreenShotEx0(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        ScreenShotAttribute attribute{};
        u32 report_option{};
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
    const auto result = manager->SaveScreenShot(entry, parameters.attribute, image_data_buffer,
                                                parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 10};
    rb.Push(result);
    rb.PushRaw(entry);
}
void IScreenShotService::SaveEditedScreenShotEx1(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        ScreenShotAttribute attribute;
        u64 width;
        u64 height;
        u64 thumbnail_width;
        u64 thumbnail_height;
        AlbumFileId file_id;
    };
    static_assert(sizeof(Parameters) == 0x78, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};
    const auto application_data_buffer = ctx.ReadBuffer(0);
    const auto image_data_buffer = ctx.ReadBuffer(1);
    const auto thumbnail_image_data_buffer = ctx.ReadBuffer(2);

    LOG_INFO(Service_Capture,
             "called, width={}, height={}, thumbnail_width={}, thumbnail_height={}, "
             "application_id={:016x},  storage={},  type={}, app_data_buffer_size={}, "
             "image_data_buffer_size={}, thumbnail_image_buffer_size={}",
             parameters.width, parameters.height, parameters.thumbnail_width,
             parameters.thumbnail_height, parameters.file_id.application_id,
             parameters.file_id.storage, parameters.file_id.type, application_data_buffer.size(),
             image_data_buffer.size(), thumbnail_image_data_buffer.size());

    ApplicationAlbumEntry entry{};
    const auto result = manager->SaveEditedScreenShot(entry, parameters.attribute,
                                                      parameters.file_id, image_data_buffer);

    IPC::ResponseBuilder rb{ctx, 10};
    rb.Push(result);
    rb.PushRaw(entry);
}

} // namespace Service::Capture
