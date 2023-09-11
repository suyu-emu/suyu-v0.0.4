// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <random>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"

#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/mii/mii_manager.h"
#include "core/hle/service/mii/mii_result.h"
#include "core/hle/service/mii/mii_util.h"
#include "core/hle/service/mii/types/core_data.h"
#include "core/hle/service/mii/types/raw_data.h"

namespace Service::Mii {
constexpr std::size_t DefaultMiiCount{RawData::DefaultMii.size()};

MiiManager::MiiManager() {}

bool MiiManager::IsUpdated(DatabaseSessionMetadata& metadata, SourceFlag source_flag) const {
    if ((source_flag & SourceFlag::Database) == SourceFlag::None) {
        return false;
    }

    const auto metadata_update_counter = metadata.update_counter;
    metadata.update_counter = update_counter;
    return metadata_update_counter != update_counter;
}

bool MiiManager::IsFullDatabase() const {
    // TODO(bunnei): We don't implement the Mii database, so it cannot be full
    return false;
}

u32 MiiManager::GetCount(const DatabaseSessionMetadata& metadata, SourceFlag source_flag) const {
    u32 mii_count{};
    if ((source_flag & SourceFlag::Default) == SourceFlag::None) {
        mii_count += DefaultMiiCount;
    }
    if ((source_flag & SourceFlag::Database) == SourceFlag::None) {
        // TODO(bunnei): We don't implement the Mii database, but when we do, update this
    }
    return mii_count;
}

Result MiiManager::UpdateLatest(DatabaseSessionMetadata& metadata, CharInfo& out_char_info,
                                const CharInfo& char_info, SourceFlag source_flag) {
    if ((source_flag & SourceFlag::Database) == SourceFlag::None) {
        return ResultNotFound;
    }

    // TODO(bunnei): We don't implement the Mii database, so we can't have an entry
    return ResultNotFound;
}

void MiiManager::BuildDefault(CharInfo& out_char_info, u32 index) const {
    StoreData store_data{};
    store_data.BuildDefault(index);
    out_char_info.SetFromStoreData(store_data);
}

void MiiManager::BuildBase(CharInfo& out_char_info, Gender gender) const {
    StoreData store_data{};
    store_data.BuildBase(gender);
    out_char_info.SetFromStoreData(store_data);
}

void MiiManager::BuildRandom(CharInfo& out_char_info, Age age, Gender gender, Race race) const {
    StoreData store_data{};
    store_data.BuildRandom(age, gender, race);
    out_char_info.SetFromStoreData(store_data);
}

void MiiManager::ConvertV3ToCharInfo(CharInfo& out_char_info, const Ver3StoreData& mii_v3) const {
    StoreData store_data{};
    mii_v3.BuildToStoreData(store_data);
    out_char_info.SetFromStoreData(store_data);
}

Result MiiManager::Get(const DatabaseSessionMetadata& metadata,
                       std::span<CharInfoElement> out_elements, u32& out_count,
                       SourceFlag source_flag) {
    if ((source_flag & SourceFlag::Database) == SourceFlag::None) {
        return BuildDefault(out_elements, out_count, source_flag);
    }

    // TODO(bunnei): We don't implement the Mii database, so we can't have an entry

    // Include default Mii at the end of the list
    return BuildDefault(out_elements, out_count, source_flag);
}

Result MiiManager::Get(const DatabaseSessionMetadata& metadata, std::span<CharInfo> out_char_info,
                       u32& out_count, SourceFlag source_flag) {
    if ((source_flag & SourceFlag::Database) == SourceFlag::None) {
        return BuildDefault(out_char_info, out_count, source_flag);
    }

    // TODO(bunnei): We don't implement the Mii database, so we can't have an entry

    // Include default Mii at the end of the list
    return BuildDefault(out_char_info, out_count, source_flag);
}

Result MiiManager::BuildDefault(std::span<CharInfoElement> out_elements, u32& out_count,
                                SourceFlag source_flag) {
    if ((source_flag & SourceFlag::Default) == SourceFlag::None) {
        return ResultSuccess;
    }

    StoreData store_data{};

    for (std::size_t index = 0; index < DefaultMiiCount; ++index) {
        if (out_elements.size() <= static_cast<std::size_t>(out_count)) {
            return ResultInvalidArgumentSize;
        }

        store_data.BuildDefault(static_cast<u32>(index));

        out_elements[out_count].source = Source::Default;
        out_elements[out_count].char_info.SetFromStoreData(store_data);
        out_count++;
    }

    return ResultSuccess;
}

Result MiiManager::BuildDefault(std::span<CharInfo> out_char_info, u32& out_count,
                                SourceFlag source_flag) {
    if ((source_flag & SourceFlag::Default) == SourceFlag::None) {
        return ResultSuccess;
    }

    StoreData store_data{};

    for (std::size_t index = 0; index < DefaultMiiCount; ++index) {
        if (out_char_info.size() <= static_cast<std::size_t>(out_count)) {
            return ResultInvalidArgumentSize;
        }

        store_data.BuildDefault(static_cast<u32>(index));

        out_char_info[out_count].SetFromStoreData(store_data);
        out_count++;
    }

    return ResultSuccess;
}

Result MiiManager::GetIndex(const DatabaseSessionMetadata& metadata, const CharInfo& char_info,
                            s32& out_index) {

    if (char_info.Verify() != 0) {
        return ResultInvalidCharInfo;
    }

    constexpr u32 INVALID_INDEX{0xFFFFFFFF};

    out_index = INVALID_INDEX;

    // TODO(bunnei): We don't implement the Mii database, so we can't have an index
    return ResultNotFound;
}

void MiiManager::SetInterfaceVersion(DatabaseSessionMetadata& metadata, u32 version) {
    metadata.interface_version = version;
}

} // namespace Service::Mii
