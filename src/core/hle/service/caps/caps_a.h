// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/fs/fs.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Capture {

class IAlbumAccessorService final : public ServiceFramework<IAlbumAccessorService> {
public:
    explicit IAlbumAccessorService(Core::System& system_);
    ~IAlbumAccessorService() override;

private:
    enum class ContentType : u8 {
        Screenshot,
        Movie,
        ExtraMovie,
    };

    enum class AlbumStorage : u8 {
        Nand,
        Sd,

    };

    enum class ScreenShotDecoderFlag : u64 {
        None = 0,
        EnableFancyUpsampling = 1 << 0,
        EnableBlockSmoothing = 1 << 1,
    };

    enum class ScreenShotOrientation : u32 {
        None,
        Rotate90,
        Rotate180,
        Rotate270,
    };

    struct ScreenShotAttribute {
        u32 unknown_0;
        ScreenShotOrientation orientation;
        u32 unknown_1;
        u32 unknown_2;
        INSERT_PADDING_BYTES(0x30);
    };
    static_assert(sizeof(ScreenShotAttribute) == 0x40, "ScreenShotAttribute is an invalid size");

    struct ScreenShotDecodeOption {
        ScreenShotDecoderFlag flags;
        INSERT_PADDING_BYTES(0x18);
    };
    static_assert(sizeof(ScreenShotDecodeOption) == 0x20,
                  "ScreenShotDecodeOption is an invalid size");

    struct AlbumFileDateTime {
        u16 year;
        u8 month;
        u8 day;
        u8 hour;
        u8 minute;
        u8 second;
        u8 unique_id;
    };
    static_assert(sizeof(AlbumFileDateTime) == 0x8, "AlbumFileDateTime is an invalid size");

    struct AlbumFileId {
        u64 application_id;
        AlbumFileDateTime date;
        AlbumStorage storage;
        ContentType type;
        INSERT_PADDING_BYTES(0x5);
        u8 unknown;
    };
    static_assert(sizeof(AlbumFileId) == 0x18, "AlbumFileId is an invalid size");

    struct AlbumEntry {
        u64 entry_size;
        AlbumFileId file_id;
    };
    static_assert(sizeof(AlbumEntry) == 0x20, "AlbumEntry is an invalid size");

    struct ApplicationData {
        std::array<u8, 0x400> data;
        u32 data_size;
    };
    static_assert(sizeof(ApplicationData) == 0x404, "ApplicationData is an invalid size");

    struct LoadAlbumScreenShotImageOutput {
        s64 width;
        s64 height;
        ScreenShotAttribute attribute;
        INSERT_PADDING_BYTES(0x400);
    };
    static_assert(sizeof(LoadAlbumScreenShotImageOutput) == 0x450,
                  "LoadAlbumScreenShotImageOutput is an invalid size");

    struct LoadAlbumScreenShotImageOutputForApplication {
        s64 width;
        s64 height;
        ScreenShotAttribute attribute;
        ApplicationData data;
        INSERT_PADDING_BYTES(0xAC);
    };
    static_assert(sizeof(LoadAlbumScreenShotImageOutputForApplication) == 0x500,
                  "LoadAlbumScreenShotImageOutput is an invalid size");

    void DeleteAlbumFile(HLERequestContext& ctx);
    void IsAlbumMounted(HLERequestContext& ctx);
    void Unknown18(HLERequestContext& ctx);
    void GetAlbumFileListEx0(HLERequestContext& ctx);
    void GetAutoSavingStorage(HLERequestContext& ctx);
    void LoadAlbumScreenShotImageEx1(HLERequestContext& ctx);
    void LoadAlbumScreenShotThumbnailImageEx1(HLERequestContext& ctx);

private:
    void FindScreenshots();
    Result GetAlbumEntry(AlbumEntry& out_entry, const std::filesystem::path& path);
    Result LoadImage(std::span<u8> out_image, const std::filesystem::path& path, int width,
                     int height, ScreenShotDecoderFlag flag);

    bool is_mounted{};
    std::vector<std::filesystem::path> sd_image_paths{};
};

} // namespace Service::Capture
