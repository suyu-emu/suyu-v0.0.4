// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/mii/mii_util.h"
#include "core/hle/service/mii/types/store_data.h"

namespace Service::Mii {
StoreData::StoreData() = default;

StoreData::StoreData(const Nickname& name, const StoreDataBitFields& bit_fields,
                     const Common::UUID& user_id) {
    core_data.name = name;
    create_id = Common::UUID::MakeRandomRFC4122V4();

    core_data.data = bit_fields;
    data_crc = MiiUtil::CalculateCrc16(&core_data.data, sizeof(core_data.data));
    device_crc = MiiUtil::CalculateCrc16(&user_id, sizeof(Common::UUID));
}

} // namespace Service::Mii
