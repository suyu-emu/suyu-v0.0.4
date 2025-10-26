// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <cstring>
#include "core/crypto/xts_encryption_layer.h"

namespace Core::Crypto {

constexpr std::size_t XTS_SECTOR_SIZE = 0x4000;

XTSEncryptionLayer::XTSEncryptionLayer(FileSys::VirtualFile base_, Key256 key_)
    : EncryptionLayer(std::move(base_)), cipher(key_, Mode::XTS) {}

std::size_t XTSEncryptionLayer::Read(u8* data, std::size_t length, std::size_t offset) const {
    if (length == 0)
        return 0;

    constexpr std::size_t PrefetchSectors = 4;

    auto* out = data;
    std::size_t remaining = length;
    std::size_t current_offset = offset;
    std::size_t total_read = 0;

    std::array<u8, XTS_SECTOR_SIZE> sector{};

    while (remaining > 0) {
        const std::size_t sector_index = current_offset / XTS_SECTOR_SIZE;
        const std::size_t sector_offset = current_offset % XTS_SECTOR_SIZE;

        const std::size_t sectors_to_read = std::min<std::size_t>(PrefetchSectors,
                                                                  (remaining + sector_offset +
                                                                   XTS_SECTOR_SIZE - 1) /
                                                                      XTS_SECTOR_SIZE);

        for (std::size_t s = 0; s < sectors_to_read && remaining > 0; ++s) {
            const std::size_t index = sector_index + s;
            const std::size_t read_offset = index * XTS_SECTOR_SIZE;
            const std::size_t got = base->Read(sector.data(), XTS_SECTOR_SIZE, read_offset);
            if (got == 0)
                return total_read;

            if (got < XTS_SECTOR_SIZE)
                std::memset(sector.data() + got, 0, XTS_SECTOR_SIZE - got);

            cipher.XTSTranscode(sector.data(), XTS_SECTOR_SIZE, sector.data(), index, XTS_SECTOR_SIZE,
                                Op::Decrypt);

            const std::size_t local_offset = (s == 0) ? sector_offset : 0;
            const std::size_t available = XTS_SECTOR_SIZE - local_offset;
            const std::size_t to_copy = std::min<std::size_t>(available, remaining);
            std::memcpy(out, sector.data() + local_offset, to_copy);

            out += to_copy;
            current_offset += to_copy;
            remaining -= to_copy;
            total_read += to_copy;
        }
    }

    return total_read;
}
} // namespace Core::Crypto
