// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include "core/hle/result.h"
#include "core/hle/service/mii/mii_types.h"
#include "core/hle/service/mii/types/char_info.h"
#include "core/hle/service/mii/types/store_data.h"
#include "core/hle/service/mii/types/ver3_store_data.h"

namespace Service::Mii {

// The Mii manager is responsible for loading and storing the Miis to the database in NAND along
// with providing an easy interface for HLE emulation of the mii service.
class MiiManager {
public:
    MiiManager();

    bool CheckAndResetUpdateCounter(SourceFlag source_flag, u64& current_update_counter);
    bool IsFullDatabase() const;
    u32 GetCount(SourceFlag source_flag) const;
    Result UpdateLatest(CharInfo* out_info, const CharInfo& info, SourceFlag source_flag);
    CharInfo BuildRandom(Age age, Gender gender, Race race);
    CharInfo BuildBase(Gender gender);
    CharInfo BuildDefault(std::size_t index);
    CharInfo ConvertV3ToCharInfo(const Ver3StoreData& mii_v3) const;
    std::vector<CharInfoElement> GetDefault(SourceFlag source_flag);
    Result GetIndex(const CharInfo& info, u32& index);

    // This is nn::mii::detail::NfpStoreDataExtentionRaw::SetFromStoreData
    NfpStoreDataExtension SetFromStoreData(const CharInfo& mii) const;

    struct MiiDatabase {
        u32 magic{}; // 'NFDB'
        std::array<StoreData, 0x64> miis{};
        INSERT_PADDING_BYTES(1);
        u8 count{};
        u16 crc{};
    };
    static_assert(sizeof(MiiDatabase) == 0x1A98, "MiiDatabase has incorrect size.");

private:
    const Common::UUID user_id{};
    u64 update_counter{};
};

}; // namespace Service::Mii
