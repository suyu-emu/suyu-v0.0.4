// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/hle/service/caps/caps_manager.h"
#include "core/hle/service/caps/caps_types.h"
#include "core/hle/service/caps/caps_u.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::Capture {

IAlbumApplicationService::IAlbumApplicationService(Core::System& system_,
                                                   std::shared_ptr<AlbumManager> album_manager)
    : ServiceFramework{system_, "caps:u"}, manager{album_manager} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {32, &IAlbumApplicationService::SetShimLibraryVersion, "SetShimLibraryVersion"},
        {102, &IAlbumApplicationService::GetAlbumFileList0AafeAruidDeprecated, "GetAlbumFileList0AafeAruidDeprecated"},
        {103, nullptr, "DeleteAlbumFileByAruid"},
        {104, nullptr, "GetAlbumFileSizeByAruid"},
        {105, nullptr, "DeleteAlbumFileByAruidForDebug"},
        {110, nullptr, "LoadAlbumScreenShotImageByAruid"},
        {120, nullptr, "LoadAlbumScreenShotThumbnailImageByAruid"},
        {130, nullptr, "PrecheckToCreateContentsByAruid"},
        {140, nullptr, "GetAlbumFileList1AafeAruidDeprecated"},
        {141, nullptr, "GetAlbumFileList2AafeUidAruidDeprecated"},
        {142, &IAlbumApplicationService::GetAlbumFileList3AaeAruid, "GetAlbumFileList3AaeAruid"},
        {143, nullptr, "GetAlbumFileList4AaeUidAruid"},
        {144, nullptr, "GetAllAlbumFileList3AaeAruid"},
        {60002, nullptr, "OpenAccessorSessionForApplication"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IAlbumApplicationService::~IAlbumApplicationService() = default;

void IAlbumApplicationService::SetShimLibraryVersion(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto library_version{rp.Pop<u64>()};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_WARNING(Service_Capture, "(STUBBED) called. library_version={}, applet_resource_user_id={}",
                library_version, applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IAlbumApplicationService::GetAlbumFileList0AafeAruidDeprecated(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto pid{rp.Pop<s32>()};
    const auto content_type{rp.PopEnum<ContentType>()};
    const auto start_posix_time{rp.Pop<s64>()};
    const auto end_posix_time{rp.Pop<s64>()};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_WARNING(Service_Capture,
                "(STUBBED) called. pid={}, content_type={}, start_posix_time={}, "
                "end_posix_time={}, applet_resource_user_id={}",
                pid, content_type, start_posix_time, end_posix_time, applet_resource_user_id);

    // TODO: Translate posix to DateTime

    std::vector<ApplicationAlbumFileEntry> entries;
    const Result result =
        manager->GetAlbumFileList(entries, content_type, {}, {}, applet_resource_user_id);

    if (!entries.empty()) {
        ctx.WriteBuffer(entries);
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push<u64>(entries.size());
}

void IAlbumApplicationService::GetAlbumFileList3AaeAruid(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto pid{rp.Pop<s32>()};
    const auto content_type{rp.PopEnum<ContentType>()};
    const auto start_date_time{rp.PopRaw<AlbumFileDateTime>()};
    const auto end_date_time{rp.PopRaw<AlbumFileDateTime>()};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_WARNING(Service_Capture,
                "(STUBBED) called. pid={}, content_type={}, applet_resource_user_id={}", pid,
                content_type, applet_resource_user_id);

    std::vector<ApplicationAlbumFileEntry> entries;
    const Result result = manager->GetAlbumFileList(entries, content_type, start_date_time,
                                                    end_date_time, applet_resource_user_id);

    if (!entries.empty()) {
        ctx.WriteBuffer(entries);
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push<u64>(entries.size());
}

} // namespace Service::Capture
