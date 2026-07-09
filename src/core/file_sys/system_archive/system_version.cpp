// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging.h"
#include "core/file_sys/system_archive/system_version.h"
#include "core/file_sys/vfs/vfs_vector.h"
#include "core/hle/api_version.h"

namespace FileSys::SystemArchive {

VirtualDir SystemVersion() {
    LOG_WARNING(Common_Filesystem, "called, using hardcoded firmware version: {}, {}", HLE::ApiVersion::VERSION_HASH, HLE::ApiVersion::DISPLAY_TITLE);

    // the "/file"
    struct VersionFileHeader {
        u8 version_major = HLE::ApiVersion::HOS_VERSION_MAJOR;
        u8 version_minor = HLE::ApiVersion::HOS_VERSION_MINOR;
        u8 version_micro = HLE::ApiVersion::HOS_VERSION_MICRO;
        std::array<u8, 1> padding_1{};
        u8 sdk_revision_major = HLE::ApiVersion::SDK_REVISION_MAJOR;
        u8 sdk_revision_minor = HLE::ApiVersion::SDK_REVISION_MINOR;
        std::array<u8, 2> padding_2{};
        std::array<u8, 0x20> platform_string{};
        std::array<u8, 0x40> version_hash{};
        std::array<u8, 0x18> display_version{};
        std::array<u8, 0x80> display_title{};
    };
    static_assert(sizeof(VersionFileHeader) == 0x100);
    VersionFileHeader version_file_header = {};
    std::memcpy(&version_file_header.platform_string, HLE::ApiVersion::PLATFORM_STRING, sizeof(HLE::ApiVersion::PLATFORM_STRING));
    std::memcpy(&version_file_header.version_hash, HLE::ApiVersion::VERSION_HASH, sizeof(HLE::ApiVersion::VERSION_HASH));
    std::memcpy(&version_file_header.display_version, HLE::ApiVersion::DISPLAY_VERSION, sizeof(HLE::ApiVersion::DISPLAY_VERSION));
    std::memcpy(&version_file_header.display_title, HLE::ApiVersion::DISPLAY_TITLE, sizeof(HLE::ApiVersion::DISPLAY_TITLE));

    std::vector<u8> file_data(sizeof(VersionFileHeader));
    std::memcpy(file_data.data(), &version_file_header, sizeof(version_file_header));
    VirtualFile file = std::make_shared<VectorVfsFile>(file_data, "file");

    // the "/digest"
    std::vector<u8> digest_data(sizeof(HLE::ApiVersion::VERSION_DIGEST));
    std::memcpy(digest_data.data(), HLE::ApiVersion::VERSION_DIGEST, sizeof(HLE::ApiVersion::VERSION_DIGEST));
    VirtualFile digest_file = std::make_shared<VectorVfsFile>(digest_data, "digest");
    return std::make_shared<VectorVfsDirectory>(std::vector<VirtualFile>{file, digest_file}, std::vector<VirtualDir>{}, "data");
}

} // namespace FileSys::SystemArchive
