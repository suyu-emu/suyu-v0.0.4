// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "aes_util.h"
#include "encryption_layer.h"
#include "key_manager.h"

namespace Crypto {

// Sits on top of a VirtualFile and provides CTR-mode AES decription.
struct CTREncryptionLayer : public EncryptionLayer {
    CTREncryptionLayer(FileSys::VirtualFile base, Key128 key, size_t base_offset);

    size_t Read(u8* data, size_t length, size_t offset) const override;

    void SetIV(std::vector<u8> iv);

private:
    size_t base_offset;

    // Must be mutable as operations modify cipher contexts.
    mutable AESCipher<Key128> cipher;
    mutable std::vector<u8> iv;

    void UpdateIV(size_t offset) const;
};

} // namespace Crypto
