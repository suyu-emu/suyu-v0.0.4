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

    std::size_t total_read = 0;
    // Handle initial unaligned part within a sector.
    if (auto const sector_offset = offset % XTS_SECTOR_SIZE; sector_offset != 0) {
        const std::size_t aligned_off = offset - sector_offset;
        std::array<u8, XTS_SECTOR_SIZE> block{};
        if (auto const got = base->Read(block.data(), XTS_SECTOR_SIZE, aligned_off); got > 0) {
            if (got < XTS_SECTOR_SIZE)
                std::memset(block.data() + got, 0, XTS_SECTOR_SIZE - got);
            cipher.XTSTranscode(block.data(), XTS_SECTOR_SIZE, block.data(), aligned_off / XTS_SECTOR_SIZE,
                                XTS_SECTOR_SIZE, Op::Decrypt);

            auto const to_copy = std::min<std::size_t>(length, got > sector_offset ? got - sector_offset : 0);
            if (to_copy > 0) {
                std::memcpy(data, block.data() + sector_offset, to_copy);
                data += to_copy;
                offset += to_copy;
                length -= to_copy;
                total_read += to_copy;
            }
        } else {
            return 0;
        }
    }

    if (length > 0) {
        // Process aligned middle inplace, in sector sized multiples.
        while (length >= XTS_SECTOR_SIZE) {
            const std::size_t req = (length / XTS_SECTOR_SIZE) * XTS_SECTOR_SIZE;
            const std::size_t got = base->Read(data, req, offset);
            if (got == 0) {
                return total_read;
            }
            const std::size_t got_rounded = got - (got % XTS_SECTOR_SIZE);
            if (got_rounded > 0) {
                cipher.XTSTranscode(data, got_rounded, data, offset / XTS_SECTOR_SIZE, XTS_SECTOR_SIZE, Op::Decrypt);
                data += got_rounded;
                offset += got_rounded;
                length -= got_rounded;
                total_read += got_rounded;
            }
            // If we didn't get a full sector next, break to handle tail.
            if (got_rounded != got) {
                break;
            }
        }
        // Handle tail within a sector, if any.
        if (length > 0) {
            std::array<u8, XTS_SECTOR_SIZE> block{};
            const std::size_t got = base->Read(block.data(), XTS_SECTOR_SIZE, offset);
            if (got > 0) {
                if (got < XTS_SECTOR_SIZE) {
                    std::memset(block.data() + got, 0, XTS_SECTOR_SIZE - got);
                }
                cipher.XTSTranscode(block.data(), XTS_SECTOR_SIZE, block.data(),
                                    offset / XTS_SECTOR_SIZE, XTS_SECTOR_SIZE, Op::Decrypt);
                const std::size_t to_copy = std::min<std::size_t>(length, got);
                std::memcpy(data, block.data(), to_copy);
                total_read += to_copy;
            }
        }
    }
    return total_read;
}
} // namespace Core::Crypto
