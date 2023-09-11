// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/mii/mii_types.h"
#include "core/hle/service/mii/types/core_data.h"

namespace Service::Mii {

struct StoreData {
    StoreData();
    StoreData(const Nickname& name, const StoreDataBitFields& bit_fields,
              const Common::UUID& user_id);

    CoreData core_data{};
    Common::UUID create_id{};
    u16 data_crc{};
    u16 device_crc{};
};
static_assert(sizeof(StoreData) == 0x44, "StoreData has incorrect size.");

struct StoreDataElement {
    StoreData store_data{};
    Source source{};
};
static_assert(sizeof(StoreDataElement) == 0x48, "StoreDataElement has incorrect size.");

}; // namespace Service::Mii
