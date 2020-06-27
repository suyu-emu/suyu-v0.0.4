// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/caps/caps.h"
#include "core/hle/service/caps/caps_u.h"

namespace Service::Capture {

class IAlbumAccessorApplicationSession final
    : public ServiceFramework<IAlbumAccessorApplicationSession> {
public:
    explicit IAlbumAccessorApplicationSession()
        : ServiceFramework{"IAlbumAccessorApplicationSession"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {2001, nullptr, "OpenAlbumMovieReadStream"},
            {2002, nullptr, "CloseAlbumMovieReadStream"},
            {2003, nullptr, "GetAlbumMovieReadStreamMovieDataSize"},
            {2004, nullptr, "ReadMovieDataFromAlbumMovieReadStream"},
            {2005, nullptr, "GetAlbumMovieReadStreamBrokenReason"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

CAPS_U::CAPS_U() : ServiceFramework("caps:u") {
    // clang-format off
    static const FunctionInfo functions[] = {
        {31, nullptr, "GetShimLibraryVersion"},
        {32, nullptr, "SetShimLibraryVersion"},
        {102, &CAPS_U::GetAlbumContentsFileListForApplication, "GetAlbumContentsFileListForApplication"},
        {103, nullptr, "DeleteAlbumContentsFileForApplication"},
        {104, nullptr, "GetAlbumContentsFileSizeForApplication"},
        {105, nullptr, "DeleteAlbumFileByAruidForDebug"},
        {110, nullptr, "LoadAlbumContentsFileScreenShotImageForApplication"},
        {120, nullptr, "LoadAlbumContentsFileThumbnailImageForApplication"},
        {130, nullptr, "PrecheckToCreateContentsForApplication"},
        {140, nullptr, "GetAlbumFileList1AafeAruidDeprecated"},
        {141, nullptr, "GetAlbumFileList2AafeUidAruidDeprecated"},
        {142, nullptr, "GetAlbumFileList3AaeAruid"},
        {143, nullptr, "GetAlbumFileList4AaeUidAruid"},
        {60002, nullptr, "OpenAccessorSessionForApplication"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

CAPS_U::~CAPS_U() = default;

void CAPS_U::GetAlbumContentsFileListForApplication(Kernel::HLERequestContext& ctx) {
    // Takes a type-0x6 output buffer containing an array of ApplicationAlbumFileEntry, a PID, an
    // u8 ContentType, two s64s, and an u64 AppletResourceUserId. Returns an output u64 for total
    // output entries (which is copied to a s32 by official SW).
    IPC::RequestParser rp{ctx};
    const auto pid{rp.Pop<s32>()};
    const auto content_type{rp.PopEnum<ContentType>()};
    const auto start_posix_time{rp.Pop<s64>()};
    const auto end_posix_time{rp.Pop<s64>()};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    // TODO: Update this when we implement the album.
    // Currently we do not have a method of accessing album entries, set this to 0 for now.
    constexpr s32 total_entries{0};

    LOG_WARNING(Service_Capture,
                "(STUBBED) called. pid={}, content_type={}, start_posix_time={}, "
                "end_posix_time={}, applet_resource_user_id={}, total_entries={}",
                pid, content_type, start_posix_time, end_posix_time, applet_resource_user_id,
                total_entries);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.Push(total_entries);
}

} // namespace Service::Capture
