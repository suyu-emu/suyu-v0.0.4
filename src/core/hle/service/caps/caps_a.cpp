// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/hle/service/caps/caps_a.h"
#include "core/hle/service/caps/caps_manager.h"
#include "core/hle/service/caps/caps_result.h"
#include "core/hle/service/caps/caps_types.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::Capture {

IAlbumAccessorService::IAlbumAccessorService(Core::System& system_,
                                             std::shared_ptr<AlbumManager> album_manager)
    : ServiceFramework{system_, "caps:a"}, manager{album_manager} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "GetAlbumFileCount"},
        {1, nullptr, "GetAlbumFileList"},
        {2, nullptr, "LoadAlbumFile"},
        {3, &IAlbumAccessorService::DeleteAlbumFile, "DeleteAlbumFile"},
        {4, nullptr, "StorageCopyAlbumFile"},
        {5, &IAlbumAccessorService::IsAlbumMounted, "IsAlbumMounted"},
        {6, nullptr, "GetAlbumUsage"},
        {7, nullptr, "GetAlbumFileSize"},
        {8, nullptr, "LoadAlbumFileThumbnail"},
        {9, nullptr, "LoadAlbumScreenShotImage"},
        {10, nullptr, "LoadAlbumScreenShotThumbnailImage"},
        {11, nullptr, "GetAlbumEntryFromApplicationAlbumEntry"},
        {12, nullptr, "LoadAlbumScreenShotImageEx"},
        {13, nullptr, "LoadAlbumScreenShotThumbnailImageEx"},
        {14, nullptr, "LoadAlbumScreenShotImageEx0"},
        {15, nullptr, "GetAlbumUsage3"},
        {16, nullptr, "GetAlbumMountResult"},
        {17, nullptr, "GetAlbumUsage16"},
        {18, &IAlbumAccessorService::Unknown18, "Unknown18"},
        {19, nullptr, "Unknown19"},
        {100, nullptr, "GetAlbumFileCountEx0"},
        {101, &IAlbumAccessorService::GetAlbumFileListEx0, "GetAlbumFileListEx0"},
        {202, nullptr, "SaveEditedScreenShot"},
        {301, nullptr, "GetLastThumbnail"},
        {302, nullptr, "GetLastOverlayMovieThumbnail"},
        {401,  &IAlbumAccessorService::GetAutoSavingStorage, "GetAutoSavingStorage"},
        {501, nullptr, "GetRequiredStorageSpaceSizeToCopyAll"},
        {1001, nullptr, "LoadAlbumScreenShotThumbnailImageEx0"},
        {1002, &IAlbumAccessorService::LoadAlbumScreenShotImageEx1, "LoadAlbumScreenShotImageEx1"},
        {1003, &IAlbumAccessorService::LoadAlbumScreenShotThumbnailImageEx1, "LoadAlbumScreenShotThumbnailImageEx1"},
        {8001, nullptr, "ForceAlbumUnmounted"},
        {8002, nullptr, "ResetAlbumMountStatus"},
        {8011, nullptr, "RefreshAlbumCache"},
        {8012, nullptr, "GetAlbumCache"},
        {8013, nullptr, "GetAlbumCacheEx"},
        {8021, nullptr, "GetAlbumEntryFromApplicationAlbumEntryAruid"},
        {10011, nullptr, "SetInternalErrorConversionEnabled"},
        {50000, nullptr, "LoadMakerNoteInfoForDebug"},
        {60002, nullptr, "OpenAccessorSession"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IAlbumAccessorService::~IAlbumAccessorService() = default;

void IAlbumAccessorService::DeleteAlbumFile(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto file_id{rp.PopRaw<AlbumFileId>()};

    LOG_INFO(Service_Capture, "called, application_id=0x{:0x}, storage={}, type={}",
             file_id.application_id, file_id.storage, file_id.type);

    Result result = manager->DeleteAlbumFile(file_id);
    result = TranslateResult(result);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IAlbumAccessorService::IsAlbumMounted(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto storage{rp.PopEnum<AlbumStorage>()};

    LOG_INFO(Service_Capture, "called, storage={}", storage);

    Result result = manager->IsAlbumMounted(storage);
    const bool is_mounted = result.IsSuccess();
    result = TranslateResult(result);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.Push<u8>(is_mounted);
}

void IAlbumAccessorService::Unknown18(HLERequestContext& ctx) {
    struct UnknownBuffer {
        INSERT_PADDING_BYTES(0x10);
    };
    static_assert(sizeof(UnknownBuffer) == 0x10, "UnknownBuffer is an invalid size");

    LOG_WARNING(Service_Capture, "(STUBBED) called");

    std::vector<UnknownBuffer> buffer{};

    if (!buffer.empty()) {
        ctx.WriteBuffer(buffer);
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(buffer.size()));
}

void IAlbumAccessorService::GetAlbumFileListEx0(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto storage{rp.PopEnum<AlbumStorage>()};
    const auto flags{rp.Pop<u8>()};
    const auto album_entry_size{ctx.GetWriteBufferNumElements<AlbumEntry>()};

    LOG_INFO(Service_Capture, "called, storage={}, flags={}", storage, flags);

    std::vector<AlbumEntry> entries;
    Result result = manager->GetAlbumFileList(entries, storage, flags);
    result = TranslateResult(result);

    entries.resize(std::min(album_entry_size, entries.size()));

    if (!entries.empty()) {
        ctx.WriteBuffer(entries);
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.Push(entries.size());
}

void IAlbumAccessorService::GetAutoSavingStorage(HLERequestContext& ctx) {
    LOG_WARNING(Service_Capture, "(STUBBED) called");

    bool is_autosaving{};
    Result result = manager->GetAutoSavingStorage(is_autosaving);
    result = TranslateResult(result);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.Push<u8>(is_autosaving);
}

void IAlbumAccessorService::LoadAlbumScreenShotImageEx1(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto file_id{rp.PopRaw<AlbumFileId>()};
    const auto decoder_options{rp.PopRaw<ScreenShotDecodeOption>()};
    const auto image_buffer_size{ctx.GetWriteBufferSize(1)};

    LOG_INFO(Service_Capture, "called, application_id=0x{:0x}, storage={}, type={}, flags={}",
             file_id.application_id, file_id.storage, file_id.type, decoder_options.flags);

    std::vector<u8> image;
    LoadAlbumScreenShotImageOutput image_output;
    Result result =
        manager->LoadAlbumScreenShotImage(image_output, image, file_id, decoder_options);
    result = TranslateResult(result);

    if (image.size() > image_buffer_size) {
        result = ResultWorkMemoryError;
    }

    if (result.IsSuccess()) {
        ctx.WriteBuffer(image_output, 0);
        ctx.WriteBuffer(image, 1);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IAlbumAccessorService::LoadAlbumScreenShotThumbnailImageEx1(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto file_id{rp.PopRaw<AlbumFileId>()};
    const auto decoder_options{rp.PopRaw<ScreenShotDecodeOption>()};

    LOG_INFO(Service_Capture, "called, application_id=0x{:0x}, storage={}, type={}, flags={}",
             file_id.application_id, file_id.storage, file_id.type, decoder_options.flags);

    std::vector<u8> image(ctx.GetWriteBufferSize(1));
    LoadAlbumScreenShotImageOutput image_output;
    Result result =
        manager->LoadAlbumScreenShotThumbnail(image_output, image, file_id, decoder_options);
    result = TranslateResult(result);

    if (result.IsSuccess()) {
        ctx.WriteBuffer(image_output, 0);
        ctx.WriteBuffer(image, 1);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

Result IAlbumAccessorService::TranslateResult(Result in_result) {
    if (in_result.IsSuccess()) {
        return in_result;
    }

    if ((in_result.raw & 0x3801ff) == ResultUnknown1024.raw) {
        if (in_result.description - 0x514 < 100) {
            return ResultInvalidFileData;
        }
        if (in_result.description - 0x5dc < 100) {
            return ResultInvalidFileData;
        }

        if (in_result.description - 0x578 < 100) {
            if (in_result == ResultFileCountLimit) {
                return ResultUnknown22;
            }
            return ResultUnknown25;
        }

        if (in_result.raw < ResultUnknown1801.raw) {
            if (in_result == ResultUnknown1202) {
                return ResultUnknown810;
            }
            if (in_result == ResultUnknown1203) {
                return ResultUnknown810;
            }
            if (in_result == ResultUnknown1701) {
                return ResultUnknown5;
            }
        } else if (in_result.raw < ResultUnknown1803.raw) {
            if (in_result == ResultUnknown1801) {
                return ResultUnknown5;
            }
            if (in_result == ResultUnknown1802) {
                return ResultUnknown6;
            }
        } else {
            if (in_result == ResultUnknown1803) {
                return ResultUnknown7;
            }
            if (in_result == ResultUnknown1804) {
                return ResultOutOfRange;
            }
        }
        return ResultUnknown1024;
    }

    if (in_result.module == ErrorModule::FS) {
        if ((in_result.description >> 0xc < 0x7d) || (in_result.description - 1000 < 2000) ||
            (((in_result.description - 3000) >> 3) < 0x271)) {
            // TODO: Translate FS error
            return in_result;
        }
    }

    return in_result;
}

} // namespace Service::Capture
