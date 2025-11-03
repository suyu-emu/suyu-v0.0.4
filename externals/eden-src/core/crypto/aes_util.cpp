// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <cstring>
#include <mbedtls/cipher.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/crypto/aes_util.h"
#include "core/crypto/key_manager.h"

namespace Core::Crypto {
namespace {
using NintendoTweak = std::array<u8, 16>;
constexpr std::size_t AesBlockBytes = 16;

NintendoTweak CalculateNintendoTweak(std::size_t sector_id) {
    NintendoTweak out{};
    for (std::size_t i = 0xF; i <= 0xF; --i) {
        out[i] = sector_id & 0xFF;
        sector_id >>= 8;
    }
    return out;
}
} // Anonymous namespace

static_assert(static_cast<std::size_t>(Mode::CTR) ==
                  static_cast<std::size_t>(MBEDTLS_CIPHER_AES_128_CTR),
              "CTR has incorrect value.");
static_assert(static_cast<std::size_t>(Mode::ECB) ==
                  static_cast<std::size_t>(MBEDTLS_CIPHER_AES_128_ECB),
              "ECB has incorrect value.");
static_assert(static_cast<std::size_t>(Mode::XTS) ==
                  static_cast<std::size_t>(MBEDTLS_CIPHER_AES_128_XTS),
              "XTS has incorrect value.");

// Structure to hide mbedtls types from header file
struct CipherContext {
    mbedtls_cipher_context_t encryption_context;
    mbedtls_cipher_context_t decryption_context;
};

template <typename Key, std::size_t KeySize>
Crypto::AESCipher<Key, KeySize>::AESCipher(Key key, Mode mode)
    : ctx(std::make_unique<CipherContext>()) {
    mbedtls_cipher_init(&ctx->encryption_context);
    mbedtls_cipher_init(&ctx->decryption_context);

    ASSERT_MSG((mbedtls_cipher_setup(
                    &ctx->encryption_context,
                    mbedtls_cipher_info_from_type(static_cast<mbedtls_cipher_type_t>(mode))) ||
                mbedtls_cipher_setup(
                    &ctx->decryption_context,
                    mbedtls_cipher_info_from_type(static_cast<mbedtls_cipher_type_t>(mode)))) == 0,
               "Failed to initialize mbedtls ciphers.");

    ASSERT(
        !mbedtls_cipher_setkey(&ctx->encryption_context, key.data(), KeySize * 8, MBEDTLS_ENCRYPT));
    ASSERT(
        !mbedtls_cipher_setkey(&ctx->decryption_context, key.data(), KeySize * 8, MBEDTLS_DECRYPT));
    //"Failed to set key on mbedtls ciphers.");
}

template <typename Key, std::size_t KeySize>
AESCipher<Key, KeySize>::~AESCipher() {
    mbedtls_cipher_free(&ctx->encryption_context);
    mbedtls_cipher_free(&ctx->decryption_context);
}

template <typename Key, std::size_t KeySize>
void AESCipher<Key, KeySize>::Transcode(const u8* src, std::size_t size, u8* dest, Op op) const {
    auto* const context = op == Op::Encrypt ? &ctx->encryption_context : &ctx->decryption_context;

    mbedtls_cipher_reset(context);

    if (size == 0)
        return;

    const auto mode = mbedtls_cipher_get_cipher_mode(context);
    std::size_t written = 0;

    if (mode != MBEDTLS_MODE_ECB) {
        const int ret = mbedtls_cipher_update(context, src, size, dest, &written);
        ASSERT(ret == 0);
        if (written != size) {
            LOG_WARNING(Crypto, "Not all data was processed requested={:016X}, actual={:016X}.", size, written);
        }
        return;
    }

    const auto block_size = mbedtls_cipher_get_block_size(context);
    ASSERT(block_size <= AesBlockBytes);

    const std::size_t whole_block_bytes = size - (size % block_size);
    if (whole_block_bytes != 0) {
        const int ret = mbedtls_cipher_update(context, src, whole_block_bytes, dest, &written);
        ASSERT(ret == 0);
        if (written != whole_block_bytes) {
            LOG_WARNING(Crypto, "Not all data was processed requested={:016X}, actual={:016X}.",
                        whole_block_bytes, written);
        }
    }

    const std::size_t tail = size - whole_block_bytes;
    if (tail == 0)
        return;

    std::array<u8, AesBlockBytes> tail_buffer{};
    std::memcpy(tail_buffer.data(), src + whole_block_bytes, tail);

    std::size_t tail_written = 0;
    const int ret = mbedtls_cipher_update(context, tail_buffer.data(), block_size, tail_buffer.data(),
                                          &tail_written);
    ASSERT(ret == 0);
    if (tail_written != block_size) {
        LOG_WARNING(Crypto, "Not all data was processed requested={:016X}, actual={:016X}.", block_size,
                    tail_written);
    }

    std::memcpy(dest + whole_block_bytes, tail_buffer.data(), tail);
}

template <typename Key, std::size_t KeySize>
void AESCipher<Key, KeySize>::XTSTranscode(const u8* src, std::size_t size, u8* dest,
                                           std::size_t sector_id, std::size_t sector_size, Op op) {
    ASSERT_MSG(size % sector_size == 0, "XTS decryption size must be a multiple of sector size.");

    for (std::size_t i = 0; i < size; i += sector_size) {
        SetIV(CalculateNintendoTweak(sector_id++));
        Transcode(src + i, sector_size, dest + i, op);
    }
}

template <typename Key, std::size_t KeySize>
void AESCipher<Key, KeySize>::SetIV(std::span<const u8> data) {
    ASSERT_MSG((mbedtls_cipher_set_iv(&ctx->encryption_context, data.data(), data.size()) ||
                mbedtls_cipher_set_iv(&ctx->decryption_context, data.data(), data.size())) == 0,
               "Failed to set IV on mbedtls ciphers.");
}

template class AESCipher<Key128>;
template class AESCipher<Key256>;
} // namespace Core::Crypto
