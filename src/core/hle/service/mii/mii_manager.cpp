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

namespace {

constexpr std::size_t DefaultMiiCount{RawData::DefaultMii.size()};

CharInfo ConvertStoreDataToInfo(const StoreData& data) {
    CharInfo char_info{};
    char_info.SetFromStoreData(data);
    return char_info;
}

StoreData BuildRandomStoreData(Age age, Gender gender, Race race, const Common::UUID& user_id) {
    StoreData store_data{};
    store_data.BuildRandom(age, gender, race);

    return store_data;
}

StoreData BuildDefaultStoreData(const DefaultMii& info, const Common::UUID& user_id) {
    StoreData store_data{};
    store_data.BuildDefault(0);

    return store_data;
}

} // namespace

MiiManager::MiiManager() : user_id{Service::Account::ProfileManager().GetLastOpenedUser()} {}

bool MiiManager::CheckAndResetUpdateCounter(SourceFlag source_flag, u64& current_update_counter) {
    if ((source_flag & SourceFlag::Database) == SourceFlag::None) {
        return false;
    }

    const bool result{current_update_counter != update_counter};

    current_update_counter = update_counter;

    return result;
}

bool MiiManager::IsFullDatabase() const {
    // TODO(bunnei): We don't implement the Mii database, so it cannot be full
    return false;
}

u32 MiiManager::GetCount(SourceFlag source_flag) const {
    std::size_t count{};
    if ((source_flag & SourceFlag::Database) != SourceFlag::None) {
        // TODO(bunnei): We don't implement the Mii database, but when we do, update this
        count += 0;
    }
    if ((source_flag & SourceFlag::Default) != SourceFlag::None) {
        count += DefaultMiiCount;
    }
    return static_cast<u32>(count);
}

Result MiiManager::UpdateLatest(CharInfo* out_info, const CharInfo& info, SourceFlag source_flag) {
    if ((source_flag & SourceFlag::Database) == SourceFlag::None) {
        return ResultNotFound;
    }

    // TODO(bunnei): We don't implement the Mii database, so we can't have an entry
    return ResultNotFound;
}

CharInfo MiiManager::BuildRandom(Age age, Gender gender, Race race) {
    return ConvertStoreDataToInfo(BuildRandomStoreData(age, gender, race, user_id));
}

CharInfo MiiManager::BuildBase(Gender gender) {
    const std::size_t index = gender == Gender::Female ? 1 : 0;
    return ConvertStoreDataToInfo(BuildDefaultStoreData(RawData::BaseMii.at(index), user_id));
}

CharInfo MiiManager::BuildDefault(std::size_t index) {
    return ConvertStoreDataToInfo(BuildDefaultStoreData(RawData::DefaultMii.at(index), user_id));
}

CharInfo MiiManager::ConvertV3ToCharInfo(const Ver3StoreData& mii_v3) const {
    CharInfo char_info{};
    StoreData store_data{};
    mii_v3.BuildToStoreData(store_data);
    char_info.SetFromStoreData(store_data);
    return char_info;
}

std::vector<CharInfoElement> MiiManager::GetDefault(SourceFlag source_flag) {
    std::vector<CharInfoElement> result;

    if ((source_flag & SourceFlag::Default) == SourceFlag::None) {
        return result;
    }

    for (std::size_t index = 0; index < DefaultMiiCount; index++) {
        result.emplace_back(BuildDefault(index), Source::Default);
    }

    return result;
}

Result MiiManager::GetIndex([[maybe_unused]] const CharInfo& info, u32& index) {
    constexpr u32 INVALID_INDEX{0xFFFFFFFF};

    index = INVALID_INDEX;

    // TODO(bunnei): We don't implement the Mii database, so we can't have an index
    return ResultNotFound;
}

} // namespace Service::Mii
