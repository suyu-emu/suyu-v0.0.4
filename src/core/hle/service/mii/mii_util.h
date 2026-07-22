// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <random>
#include <span>

#include "common/common_types.h"
#include "common/random.h"
#include "common/swap.h"
#include "common/uuid.h"
#include "core/hle/service/mii/mii_types.h"

namespace Service::Mii {
class MiiUtil {
public:
    static u16 CalculateCrc16(const void* data, std::size_t size) {
        s32 crc{};
        for (std::size_t i = 0; i < size; i++) {
            crc ^= static_cast<const u8*>(data)[i] << 8;
            for (std::size_t j = 0; j < 8; j++) {
                crc <<= 1;
                if ((crc & 0x10000) != 0) {
                    crc = (crc ^ 0x1021) & 0xFFFF;
                }
            }
        }
        return Common::swap16(static_cast<u16>(crc));
    }

    static u16 CalculateDeviceCrc16(const Common::UUID& uuid, std::size_t data_size) {
        constexpr u16 magic{0x1021};
        s32 crc{};

        for (std::size_t i = 0; i < uuid.uuid.size(); i++) {
            for (std::size_t j = 0; j < 8; j++) {
                crc <<= 1;
                if ((crc & 0x10000) != 0) {
                    crc = crc ^ magic;
                }
            }
            crc ^= uuid.uuid[i];
        }

        // As much as this looks wrong this is what N's does

        for (std::size_t i = 0; i < data_size * 8; i++) {
            crc <<= 1;
            if ((crc & 0x10000) != 0) {
                crc = crc ^ magic;
            }
        }

        return Common::swap16(static_cast<u16>(crc));
    }

    static Common::UUID MakeCreateId() {
        return Common::UUID::MakeRandomRFC4122V4();
    }

    static Common::UUID GetDeviceId() {
        // This should be nn::settings::detail::GetMiiAuthorId()
        return Common::UUID::MakeDefault();
    }

    template <typename T>
    static T GetRandomValue(T min, T max) {
        std::uniform_int_distribution<u64> distribution{u64(min), u64(max)};
        auto gen = Common::Random::GetMT19937();
        return T(distribution(gen));
    }

    template <typename T>
    static T GetRandomValue(T max) {
        return GetRandomValue<T>({}, max);
    }

    static bool IsFontRegionValid(FontRegion font, std::span<const char16_t> text) {
        // TODO: This function needs to check against the font tables
        return true;
    }
};
} // namespace Service::Mii
