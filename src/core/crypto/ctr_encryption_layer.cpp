// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>
#include "core/crypto/ctr_encryption_layer.h"

namespace Core::Crypto {

CTREncryptionLayer::CTREncryptionLayer(FileSys::VirtualFile base_, Key128 key_,
                                       std::size_t base_offset_)
    : EncryptionLayer(std::move(base_)), base_offset(base_offset_), cipher(key_, Mode::CTR) {}

std::size_t CTREncryptionLayer::Read(u8* data, std::size_t length, std::size_t offset) const {
    if (length == 0)
        return 0;

    std::size_t total_read = 0;
    // Handle an initial misaligned portion if needed.
    if (auto const sector_offset = offset & 0xF; sector_offset != 0) {
        const std::size_t aligned_off = offset - sector_offset;
        std::array<u8, 0x10> block{};
        if (auto const got = base->Read(block.data(), block.size(), aligned_off); got != 0) {
            UpdateIV(base_offset + aligned_off);
            cipher.Transcode(block.data(), got, block.data(), Op::Decrypt);
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
        // Now aligned to 0x10
        UpdateIV(base_offset + offset);
        const std::size_t got = base->Read(data, length, offset);
        if (got > 0) {
            cipher.Transcode(data, got, data, Op::Decrypt);
            total_read += got;
        }
    }
    return total_read;
}

void CTREncryptionLayer::SetIV(const IVData& iv_) {
    iv = iv_;
}

void CTREncryptionLayer::UpdateIV(std::size_t offset) const {
    offset >>= 4;
    for (std::size_t i = 0; i < 8; ++i) {
        iv[16 - i - 1] = offset & 0xFF;
        offset >>= 8;
    }
    cipher.SetIV(iv);
}
} // namespace Core::Crypto
