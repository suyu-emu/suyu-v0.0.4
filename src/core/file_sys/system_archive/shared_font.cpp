// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/file_sys/system_archive/data/font_chinese_simplified.h"
#include "core/file_sys/system_archive/data/font_chinese_traditional.h"
#include "core/file_sys/system_archive/data/font_extended_chinese_simplified.h"
#include "core/file_sys/system_archive/data/font_korean.h"
#include "core/file_sys/system_archive/data/font_nintendo_extended.h"
#include "core/file_sys/system_archive/data/font_standard.h"
#include "core/file_sys/system_archive/shared_font.h"
#include "core/file_sys/vfs_vector.h"
#include "core/hle/service/ns/pl_u.h"

namespace FileSys::SystemArchive {

namespace {

template <std::size_t Size>
VirtualFile PackBFTTF(const std::array<u8, Size>& data, const std::string& name) {
    std::vector<u8> vec(Size);
    std::memcpy(vec.data(), data.data(), vec.size());

    Kernel::PhysicalMemory bfttf(Size + sizeof(u64));

    Service::NS::EncryptSharedFont(vec, bfttf);

    std::vector<u8> bfttf_vec(Size + sizeof(u64));
    std::memcpy(bfttf_vec.data(), bfttf.data(), bfttf_vec.size());
    return std::make_shared<VectorVfsFile>(std::move(bfttf_vec), name);
}

} // Anonymous namespace

VirtualDir FontNintendoExtension() {
    return std::make_shared<VectorVfsDirectory>(
        std::vector<VirtualFile>{
            PackBFTTF(SharedFontData::FONT_NINTENDO_EXTENDED, "nintendo_ext_003.bfttf"),
            PackBFTTF(SharedFontData::FONT_NINTENDO_EXTENDED, "nintendo_ext2_003.bfttf"),
        },
        std::vector<VirtualDir>{});
}

VirtualDir FontStandard() {
    return std::make_shared<VectorVfsDirectory>(
        std::vector<VirtualFile>{
            PackBFTTF(SharedFontData::FONT_STANDARD, "nintendo_udsg-r_std_003.bfttf"),
        },
        std::vector<VirtualDir>{});
}

VirtualDir FontKorean() {
    return std::make_shared<VectorVfsDirectory>(
        std::vector<VirtualFile>{
            PackBFTTF(SharedFontData::FONT_KOREAN, "nintendo_udsg-r_ko_003.bfttf"),
        },
        std::vector<VirtualDir>{});
}

VirtualDir FontChineseTraditional() {
    return std::make_shared<VectorVfsDirectory>(
        std::vector<VirtualFile>{
            PackBFTTF(SharedFontData::FONT_CHINESE_TRADITIONAL,
                      "nintendo_udjxh-db_zh-tw_003.bfttf"),
        },
        std::vector<VirtualDir>{});
}

VirtualDir FontChineseSimple() {
    return std::make_shared<VectorVfsDirectory>(
        std::vector<VirtualFile>{
            PackBFTTF(SharedFontData::FONT_CHINESE_SIMPLIFIED,
                      "nintendo_udsg-r_org_zh-cn_003.bfttf"),
            PackBFTTF(SharedFontData::FONT_EXTENDED_CHINESE_SIMPLIFIED,
                      "nintendo_udsg-r_ext_zh-cn_003.bfttf"),
        },
        std::vector<VirtualDir>{});
}

} // namespace FileSys::SystemArchive
