// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_funcs.h"
#include "common/uuid.h"
#include "core/file_sys/romfs_factory.h"

namespace Service::NS {

enum class ApplicationRecordType : u8 {
    Installing = 2,
    Installed = 3,
    GameCardNotInserted = 5,
    Archived = 11,
    GameCard = 16,
};

enum class ApplicationControlSource : u8 {
    CacheOnly = 0,
    Storage = 1,
    StorageOnly = 2,
};

enum class BackgroundNetworkUpdateState : u8 {
    None,
    InProgress,
    Ready,
};

struct ApplicationRecord {
    u64 application_id;
    ApplicationRecordType type;
    u8 unknown;
    INSERT_PADDING_BYTES_NOINIT(0x6);
    u8 unknown2;
    INSERT_PADDING_BYTES_NOINIT(0x7);
};
static_assert(sizeof(ApplicationRecord) == 0x18, "ApplicationRecord has incorrect size.");

/// ApplicationDownloadState
struct ApplicationDownloadState {
    u64 downloaded_size;
    u64 total_size;
    u32 unk_x10;
    u8 state;
    u8 unk_x19;
    std::array<u8, 0x2> unk_x1a;
    u64 unk_x20;
};
static_assert(sizeof(ApplicationDownloadState) == 0x20,
              "ApplicationDownloadState has incorrect size.");

/// ApplicationView
struct ApplicationView {
    u64 application_id;                         ///< ApplicationId.
    u32 version;                                ///< Application Version(?)
    u32 flags;                                  ///< Flags.
    ApplicationDownloadState download_state;    ///< \ref ApplicationDownloadState
    ApplicationDownloadState download_progress; ///< \ref ApplicationDownloadState
};
static_assert(sizeof(ApplicationView) == 0x50, "ApplicationView has incorrect size.");

struct ApplicationRightsOnClient {
    u64 application_id;
    Common::UUID uid;
    u8 flags;
    u8 flags2;
    INSERT_PADDING_BYTES_NOINIT(0x6);
};
static_assert(sizeof(ApplicationRightsOnClient) == 0x20,
              "ApplicationRightsOnClient has incorrect size.");

/// NsPromotionInfo
struct PromotionInfo {
    u64 start_timestamp; ///< POSIX timestamp for the promotion start.
    u64 end_timestamp;   ///< POSIX timestamp for the promotion end.
    s64 remaining_time;  ///< Remaining time until the promotion ends, in nanoseconds
                         ///< ({end_timestamp - current_time} converted to nanoseconds).
    INSERT_PADDING_BYTES_NOINIT(0x4);
    u8 flags; ///< Flags. Bit0: whether the PromotionInfo is valid (including bit1). Bit1 clear:
              ///< remaining_time is set.
    INSERT_PADDING_BYTES_NOINIT(0x3);
};
static_assert(sizeof(PromotionInfo) == 0x20, "PromotionInfo has incorrect size.");

// TODO(Maufeat): NsApplicationViewWithPromotionInfo is on SDK20+ 0x78 bytes
/// NsApplicationViewWithPromotionInfo
struct ApplicationViewWithPromotionInfo {
    ApplicationView view;           ///< \ref NsApplicationView
    PromotionInfo promotion;        ///< \ref NsPromotionInfo
};
static_assert(sizeof(ApplicationViewWithPromotionInfo) == 0x70,
              "ApplicationViewWithPromotionInfo has incorrect size.");

struct ApplicationOccupiedSizeEntity {
    FileSys::StorageId storage_id;
    u64 app_size;
    u64 patch_size;
    u64 aoc_size;
};
static_assert(sizeof(ApplicationOccupiedSizeEntity) == 0x20,
              "ApplicationOccupiedSizeEntity has incorrect size.");

struct ApplicationOccupiedSize {
    std::array<ApplicationOccupiedSizeEntity, 4> entities;
};
static_assert(sizeof(ApplicationOccupiedSize) == 0x80,
              "ApplicationOccupiedSize has incorrect size.");

struct ContentPath {
    u8 file_system_proxy_type;
    u64 program_id;
};
static_assert(sizeof(ContentPath) == 0x10, "ContentPath has incorrect size.");

struct Uid {
    alignas(8) Common::UUID uuid;
};
static_assert(sizeof(Uid) == 0x10, "Uid has incorrect size.");

struct ApplicationDisplayData {
    std::array<char, 0x200> application_name;
    std::array<char, 0x100> developer_name;
};
static_assert(sizeof(ApplicationDisplayData) == 0x300, "ApplicationDisplayData has incorrect size.");

} // namespace Service::NS
