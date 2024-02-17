// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_funcs.h"

namespace Service::NS {

enum class ApplicationRecordType : u8 {
    Installing = 2,
    Installed = 3,
    GameCardNotInserted = 5,
    Archived = 0xB,
    GameCard = 0x10,
};

struct ApplicationRecord {
    u64 application_id;
    ApplicationRecordType type;
    u8 unknown;
    INSERT_PADDING_BYTES_NOINIT(0x6);
    u8 unknown2;
    INSERT_PADDING_BYTES_NOINIT(0x7);
};
static_assert(sizeof(ApplicationRecord) == 0x18, "ApplicationRecord is an invalid size");

/// ApplicationView
struct ApplicationView {
    u64 application_id; ///< ApplicationId.
    u32 unk;            ///< Unknown.
    u32 flags;          ///< Flags.
    u8 unk_x10[0x10];   ///< Unknown.
    u32 unk_x20;        ///< Unknown.
    u16 unk_x24;        ///< Unknown.
    u8 unk_x26[0x2];    ///< Unknown.
    u8 unk_x28[0x8];    ///< Unknown.
    u8 unk_x30[0x10];   ///< Unknown.
    u32 unk_x40;        ///< Unknown.
    u8 unk_x44;         ///< Unknown.
    u8 unk_x45[0xb];    ///< Unknown.
};

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

/// NsApplicationViewWithPromotionInfo
struct ApplicationViewWithPromotionInfo {
    ApplicationView view;    ///< \ref NsApplicationView
    PromotionInfo promotion; ///< \ref NsPromotionInfo
};

} // namespace Service::NS
