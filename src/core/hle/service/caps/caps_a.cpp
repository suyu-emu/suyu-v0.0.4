// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <sstream>
#include <stb_image.h>
#include <stb_image_resize.h>

#include "common/fs/file.h"
#include "common/fs/path_util.h"
#include "core/hle/service/caps/caps_a.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::Capture {

class IAlbumAccessorSession final : public ServiceFramework<IAlbumAccessorSession> {
public:
    explicit IAlbumAccessorSession(Core::System& system_)
        : ServiceFramework{system_, "IAlbumAccessorSession"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {2001, nullptr, "OpenAlbumMovieReadStream"},
            {2002, nullptr, "CloseAlbumMovieReadStream"},
            {2003, nullptr, "GetAlbumMovieReadStreamMovieDataSize"},
            {2004, nullptr, "ReadMovieDataFromAlbumMovieReadStream"},
            {2005, nullptr, "GetAlbumMovieReadStreamBrokenReason"},
            {2006, nullptr, "GetAlbumMovieReadStreamImageDataSize"},
            {2007, nullptr, "ReadImageDataFromAlbumMovieReadStream"},
            {2008, nullptr, "ReadFileAttributeFromAlbumMovieReadStream"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

IAlbumAccessorService::IAlbumAccessorService(Core::System& system_)
    : ServiceFramework{system_, "caps:a"} {
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

    if (file_id.storage == AlbumStorage::Sd) {
        if (!Common::FS::RemoveFile(sd_image_paths[file_id.date.unique_id])) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ResultUnknown);
            return;
        }
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IAlbumAccessorService::IsAlbumMounted(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto storage{rp.PopEnum<AlbumStorage>()};

    LOG_INFO(Service_Capture, "called, storage={}, is_mounted={}", storage, is_mounted);

    if (storage == AlbumStorage::Sd) {
        FindScreenshots();
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
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

    LOG_INFO(Service_Capture, "called, storage={}, flags={}", storage, flags);

    std::vector<AlbumEntry> entries{};

    if (storage == AlbumStorage::Sd) {
        AlbumEntry entry;
        for (u8 i = 0; i < static_cast<u8>(sd_image_paths.size()); i++) {
            if (GetAlbumEntry(entry, sd_image_paths[i]).IsError()) {
                continue;
            }
            entry.file_id.date.unique_id = i;
            entries.push_back(entry);
        }
    }

    if (!entries.empty()) {
        ctx.WriteBuffer(entries);
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(entries.size());
}

void IAlbumAccessorService::GetAutoSavingStorage(HLERequestContext& ctx) {
    bool is_autosaving{};

    LOG_WARNING(Service_Capture, "(STUBBED) called, is_autosaving={}", is_autosaving);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(is_autosaving);
}

void IAlbumAccessorService::LoadAlbumScreenShotImageEx1(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto file_id{rp.PopRaw<AlbumFileId>()};
    const auto decoder_options{rp.PopRaw<ScreenShotDecodeOption>()};

    LOG_INFO(Service_Capture, "called, application_id=0x{:0x}, storage={}, type={}, flags={}",
             file_id.application_id, file_id.storage, file_id.type, decoder_options.flags);

    const LoadAlbumScreenShotImageOutput image_output{
        .width = 1280,
        .height = 720,
        .attribute =
            {
                .unknown_0{},
                .orientation = ScreenShotOrientation::None,
                .unknown_1{},
                .unknown_2{},
            },
    };

    std::vector<u8> image(image_output.height * image_output.width * STBI_rgb_alpha);

    if (file_id.storage == AlbumStorage::Sd) {
        LoadImage(image, sd_image_paths[file_id.date.unique_id],
                  static_cast<int>(image_output.width), static_cast<int>(image_output.height),
                  decoder_options.flags);
    }

    ctx.WriteBuffer(image_output, 0);
    ctx.WriteBuffer(image, 1);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IAlbumAccessorService::LoadAlbumScreenShotThumbnailImageEx1(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto file_id{rp.PopRaw<AlbumFileId>()};
    const auto decoder_options{rp.PopRaw<ScreenShotDecodeOption>()};

    LOG_INFO(Service_Capture, "called, application_id=0x{:0x}, storage={}, type={}, flags={}",
             file_id.application_id, file_id.storage, file_id.type, decoder_options.flags);

    const LoadAlbumScreenShotImageOutput image_output{
        .width = 320,
        .height = 180,
        .attribute =
            {
                .unknown_0{},
                .orientation = ScreenShotOrientation::None,
                .unknown_1{},
                .unknown_2{},
            },
    };

    std::vector<u8> image(image_output.height * image_output.width * STBI_rgb_alpha);

    if (file_id.storage == AlbumStorage::Sd) {
        LoadImage(image, sd_image_paths[file_id.date.unique_id],
                  static_cast<int>(image_output.width), static_cast<int>(image_output.height),
                  decoder_options.flags);
    }

    ctx.WriteBuffer(image_output, 0);
    ctx.WriteBuffer(image, 1);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IAlbumAccessorService::FindScreenshots() {
    is_mounted = false;
    sd_image_paths.clear();

    // TODO: Swap this with a blocking operation.
    const auto screenshots_dir = Common::FS::GetYuzuPath(Common::FS::YuzuPath::ScreenshotsDir);
    Common::FS::IterateDirEntries(
        screenshots_dir,
        [this](const std::filesystem::path& full_path) {
            AlbumEntry entry;
            // TODO: Implement proper indexing to allow more images
            if (sd_image_paths.size() > 0xFF) {
                return true;
            }
            if (GetAlbumEntry(entry, full_path).IsSuccess()) {
                sd_image_paths.push_back(full_path);
            }
            return true;
        },
        Common::FS::DirEntryFilter::File);

    is_mounted = true;
}

Result IAlbumAccessorService::GetAlbumEntry(AlbumEntry& out_entry,
                                            const std::filesystem::path& path) {
    std::istringstream line_stream(path.filename().string());
    std::string date;
    std::string application;
    std::string time;

    // Parse filename to obtain entry properties
    std::getline(line_stream, application, '_');
    std::getline(line_stream, date, '_');
    std::getline(line_stream, time, '_');

    std::istringstream date_stream(date);
    std::istringstream time_stream(time);
    std::string year;
    std::string month;
    std::string day;
    std::string hour;
    std::string minute;
    std::string second;

    std::getline(date_stream, year, '-');
    std::getline(date_stream, month, '-');
    std::getline(date_stream, day, '-');

    std::getline(time_stream, hour, '-');
    std::getline(time_stream, minute, '-');
    std::getline(time_stream, second, '-');

    try {
        out_entry = {
            .entry_size = 1,
            .file_id{
                .application_id = static_cast<u64>(std::stoll(application, 0, 16)),
                .date =
                    {
                        .year = static_cast<u16>(std::stoi(year)),
                        .month = static_cast<u8>(std::stoi(month)),
                        .day = static_cast<u8>(std::stoi(day)),
                        .hour = static_cast<u8>(std::stoi(hour)),
                        .minute = static_cast<u8>(std::stoi(minute)),
                        .second = static_cast<u8>(std::stoi(second)),
                        .unique_id = 0,
                    },
                .storage = AlbumStorage::Sd,
                .type = ContentType::Screenshot,
                .unknown = 1,
            },
        };
    } catch (const std::invalid_argument&) {
        return ResultUnknown;
    } catch (const std::out_of_range&) {
        return ResultUnknown;
    } catch (const std::exception&) {
        return ResultUnknown;
    }

    return ResultSuccess;
}

Result IAlbumAccessorService::LoadImage(std::span<u8> out_image, const std::filesystem::path& path,
                                        int width, int height, ScreenShotDecoderFlag flag) {
    if (out_image.size() != static_cast<std::size_t>(width * height * STBI_rgb_alpha)) {
        return ResultUnknown;
    }

    const Common::FS::IOFile db_file{path, Common::FS::FileAccessMode::Read,
                                     Common::FS::FileType::BinaryFile};

    std::vector<u8> raw_file(db_file.GetSize());
    if (db_file.Read(raw_file) != raw_file.size()) {
        return ResultUnknown;
    }

    int filter_flag = STBIR_FILTER_DEFAULT;
    int original_width, original_height, color_channels;
    const auto dbi_image =
        stbi_load_from_memory(raw_file.data(), static_cast<int>(raw_file.size()), &original_width,
                              &original_height, &color_channels, STBI_rgb_alpha);

    if (dbi_image == nullptr) {
        return ResultUnknown;
    }

    switch (flag) {
    case ScreenShotDecoderFlag::EnableFancyUpsampling:
        filter_flag = STBIR_FILTER_TRIANGLE;
        break;
    case ScreenShotDecoderFlag::EnableBlockSmoothing:
        filter_flag = STBIR_FILTER_BOX;
        break;
    default:
        filter_flag = STBIR_FILTER_DEFAULT;
        break;
    }

    stbir_resize_uint8_srgb(dbi_image, original_width, original_height, 0, out_image.data(), width,
                            height, 0, STBI_rgb_alpha, 3, filter_flag);

    return ResultSuccess;
}

} // namespace Service::Capture
