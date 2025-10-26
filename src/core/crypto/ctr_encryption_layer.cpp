// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <cstring>
#include "core/crypto/ctr_encryption_layer.h"

namespace Core::Crypto {

CTREncryptionLayer::CTREncryptionLayer(FileSys::VirtualFile base_, Key128 key_,
                                       std::size_t base_offset_)
    : EncryptionLayer(std::move(base_)), base_offset(base_offset_), cipher(key_, Mode::CTR) {}

std::size_t CTREncryptionLayer::Read(u8* data, std::size_t length, std::size_t offset) const {
    if (length == 0)
        return 0;

    constexpr std::size_t BlockSize = 0x10;
    constexpr std::size_t MaxChunkSize = 0x10000;

    std::size_t total_read = 0;
    auto* out = data;
    std::size_t remaining = length;
    std::size_t current_offset = offset;

    const auto read_exact = [this](u8* dst, std::size_t bytes, std::size_t src_offset) {
        std::size_t filled = 0;
        while (filled < bytes) {
            const std::size_t got = base->Read(dst + filled, bytes - filled, src_offset + filled);
            if (got == 0)
                break;
            filled += got;
        }
        return filled;
    };

    if (const std::size_t intra_block = current_offset & (BlockSize - 1); intra_block != 0) {
        std::array<u8, BlockSize> block{};
        const std::size_t aligned_offset = current_offset - intra_block;
        const std::size_t got = read_exact(block.data(), BlockSize, aligned_offset);
        if (got <= intra_block)
            return total_read;

        UpdateIV(base_offset + aligned_offset);
        cipher.Transcode(block.data(), got, block.data(), Op::Decrypt);

        const std::size_t available = got - intra_block;
        const std::size_t to_copy = std::min<std::size_t>(remaining, available);
        std::memcpy(out, block.data() + intra_block, to_copy);

        out += to_copy;
        current_offset += to_copy;
        remaining -= to_copy;
        total_read += to_copy;

        if (to_copy != available)
            return total_read;
    }

    while (remaining >= BlockSize) {
        const std::size_t chunk_request = std::min<std::size_t>(remaining, MaxChunkSize);
        const std::size_t aligned_request = chunk_request - (chunk_request % BlockSize);
        if (aligned_request == 0)
            break;

        const std::size_t got = read_exact(out, aligned_request, current_offset);
        if (got == 0)
            break;

        UpdateIV(base_offset + current_offset);
        cipher.Transcode(out, got, out, Op::Decrypt);

        out += got;
        current_offset += got;
        remaining -= got;
        total_read += got;

        if (got < aligned_request)
            return total_read;
    }

    if (remaining > 0) {
        std::array<u8, BlockSize> block{};
        const std::size_t got = read_exact(block.data(), BlockSize, current_offset);
        if (got == 0)
            return total_read;

        UpdateIV(base_offset + current_offset);
        cipher.Transcode(block.data(), got, block.data(), Op::Decrypt);

        const std::size_t to_copy = std::min<std::size_t>(remaining, got);
        std::memcpy(out, block.data(), to_copy);
        total_read += to_copy;
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
