// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/assert.h"
#include "core/crypto/xts_encryption_layer.h"

namespace Core::Crypto {

XTSEncryptionLayer::XTSEncryptionLayer(FileSys::VirtualFile base_, Key256 key_)
    : EncryptionLayer(std::move(base_)), cipher(key_, Mode::XTS) {}

size_t XTSEncryptionLayer::Read(u8* data, size_t length, size_t offset) const {
    if (length == 0)
        return 0;

    const auto sector_offset = offset & 0x3FFF;
    if (sector_offset == 0) {
        if (length % 0x4000 == 0) {
            std::vector<u8> raw = base->ReadBytes(length, offset);
            cipher.XTSTranscode(raw.data(), raw.size(), data, offset / 0x4000, 0x4000, Op::Decrypt);
            return raw.size();
        }
        if (length > 0x4000) {
            const auto rem = length % 0x4000;
            const auto read = length - rem;
            return Read(data, read, offset) + Read(data + read, rem, offset + read);
        }
        std::vector<u8> buffer = base->ReadBytes(0x4000, offset);
        if (buffer.size() < 0x4000)
            buffer.resize(0x4000);
        cipher.XTSTranscode(buffer.data(), buffer.size(), buffer.data(), offset / 0x4000, 0x4000,
                            Op::Decrypt);
        std::memcpy(data, buffer.data(), std::min(buffer.size(), length));
        return std::min(buffer.size(), length);
    }

    // offset does not fall on block boundary (0x4000)
    std::vector<u8> block = base->ReadBytes(0x4000, offset - sector_offset);
    if (block.size() < 0x4000)
        block.resize(0x4000);
    cipher.XTSTranscode(block.data(), block.size(), block.data(), (offset - sector_offset) / 0x4000,
                        0x4000, Op::Decrypt);
    const size_t read = 0x4000 - sector_offset;

    if (length + sector_offset < 0x4000) {
        std::memcpy(data, block.data() + sector_offset, std::min<u64>(length, read));
        return std::min<u64>(length, read);
    }
    std::memcpy(data, block.data() + sector_offset, read);
    return read + Read(data + read, length - read, offset + read);
}
} // namespace Core::Crypto
