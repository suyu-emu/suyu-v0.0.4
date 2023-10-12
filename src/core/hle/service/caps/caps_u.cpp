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
    struct Parameters {
        ContentType content_type;
        INSERT_PADDING_BYTES(7);
        s64 start_posix_time;
        s64 end_posix_time;
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x20, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_WARNING(Service_Capture,
                "(STUBBED) called. content_type={}, start_posix_time={}, end_posix_time={}, "
                "applet_resource_user_id={}",
                parameters.content_type, parameters.start_posix_time, parameters.end_posix_time,
                parameters.applet_resource_user_id);

    Result result = ResultSuccess;

    if (result.IsSuccess()) {
        result = manager->IsAlbumMounted(AlbumStorage::Sd);
    }

    std::vector<ApplicationAlbumFileEntry> entries;
    if (result.IsSuccess()) {
        result = manager->GetAlbumFileList(entries, parameters.content_type,
                                           parameters.start_posix_time, parameters.end_posix_time,
                                           parameters.applet_resource_user_id);
    }

    if (!entries.empty()) {
        ctx.WriteBuffer(entries);
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push<u64>(entries.size());
}

void IAlbumApplicationService::GetAlbumFileList3AaeAruid(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        ContentType content_type;
        INSERT_PADDING_BYTES(1);
        AlbumFileDateTime start_date_time;
        AlbumFileDateTime end_date_time;
        INSERT_PADDING_BYTES(6);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x20, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_WARNING(Service_Capture,
                "(STUBBED) called. content_type={}, start_date={}/{}/{}, "
                "end_date={}/{}/{}, applet_resource_user_id={}",
                parameters.content_type, parameters.start_date_time.year,
                parameters.start_date_time.month, parameters.start_date_time.day,
                parameters.end_date_time.year, parameters.end_date_time.month,
                parameters.end_date_time.day, parameters.applet_resource_user_id);

    Result result = ResultSuccess;

    if (result.IsSuccess()) {
        result = manager->IsAlbumMounted(AlbumStorage::Sd);
    }

    std::vector<ApplicationAlbumEntry> entries;
    if (result.IsSuccess()) {
        result =
            manager->GetAlbumFileList(entries, parameters.content_type, parameters.start_date_time,
                                      parameters.end_date_time, parameters.applet_resource_user_id);
    }

    if (!entries.empty()) {
        ctx.WriteBuffer(entries);
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push<u64>(entries.size());
}

} // namespace Service::Capture
