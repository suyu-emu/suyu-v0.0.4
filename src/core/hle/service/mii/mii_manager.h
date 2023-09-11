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

    bool IsUpdated(DatabaseSessionMetadata& metadata, SourceFlag source_flag) const;

    bool IsFullDatabase() const;
    u32 GetCount(const DatabaseSessionMetadata& metadata, SourceFlag source_flag) const;
    Result UpdateLatest(DatabaseSessionMetadata& metadata, CharInfo& out_char_info,
                        const CharInfo& char_info, SourceFlag source_flag);
    Result Get(const DatabaseSessionMetadata& metadata, std::span<CharInfoElement> out_elements,
               u32& out_count, SourceFlag source_flag);
    Result Get(const DatabaseSessionMetadata& metadata, std::span<CharInfo> out_char_info,
               u32& out_count, SourceFlag source_flag);
    void BuildDefault(CharInfo& out_char_info, u32 index) const;
    void BuildBase(CharInfo& out_char_info, Gender gender) const;
    void BuildRandom(CharInfo& out_char_info, Age age, Gender gender, Race race) const;
    void ConvertV3ToCharInfo(CharInfo& out_char_info, const Ver3StoreData& mii_v3) const;
    std::vector<CharInfoElement> GetDefault(SourceFlag source_flag);
    Result GetIndex(const DatabaseSessionMetadata& metadata, const CharInfo& char_info,
                    s32& out_index);
    void SetInterfaceVersion(DatabaseSessionMetadata& metadata, u32 version);

    struct MiiDatabase {
        u32 magic{}; // 'NFDB'
        std::array<StoreData, 0x64> miis{};
        INSERT_PADDING_BYTES(1);
        u8 count{};
        u16 crc{};
    };
    static_assert(sizeof(MiiDatabase) == 0x1A98, "MiiDatabase has incorrect size.");

private:
    Result BuildDefault(std::span<CharInfoElement> out_elements, u32& out_count,
                        SourceFlag source_flag);
    Result BuildDefault(std::span<CharInfo> out_char_info, u32& out_count, SourceFlag source_flag);

    u64 update_counter{};
};

}; // namespace Service::Mii
