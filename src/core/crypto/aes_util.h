// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/assert.h"
#include "core/file_sys/vfs.h"
#include "mbedtls/cipher.h"

namespace Crypto {

enum class Mode {
    CTR = MBEDTLS_CIPHER_AES_128_CTR,
    ECB = MBEDTLS_CIPHER_AES_128_ECB,
    XTS = MBEDTLS_CIPHER_AES_128_XTS,
};

enum class Op {
    ENCRYPT,
    DECRYPT,
};

template <typename Key, size_t KeySize = sizeof(Key)>
struct AESCipher {
    static_assert(std::is_same_v<Key, std::array<u8, KeySize>>, "Key must be std::array of u8.");
    static_assert(KeySize == 0x10 || KeySize == 0x20, "KeySize must be 128 or 256.");

    AESCipher(Key key, Mode mode) {
        mbedtls_cipher_init(&encryption_context);
        mbedtls_cipher_init(&decryption_context);

        ASSERT_MSG((mbedtls_cipher_setup(
                        &encryption_context,
                        mbedtls_cipher_info_from_type(static_cast<mbedtls_cipher_type_t>(mode))) ||
                    mbedtls_cipher_setup(&decryption_context,
                                         mbedtls_cipher_info_from_type(
                                             static_cast<mbedtls_cipher_type_t>(mode)))) == 0,
                   "Failed to initialize mbedtls ciphers.");

        ASSERT(
            !mbedtls_cipher_setkey(&encryption_context, key.data(), KeySize * 8, MBEDTLS_ENCRYPT));
        ASSERT(
            !mbedtls_cipher_setkey(&decryption_context, key.data(), KeySize * 8, MBEDTLS_DECRYPT));
        //"Failed to set key on mbedtls ciphers.");
    }

    ~AESCipher() {
        mbedtls_cipher_free(&encryption_context);
        mbedtls_cipher_free(&decryption_context);
    }

    void SetIV(std::vector<u8> iv) {
        ASSERT_MSG((mbedtls_cipher_set_iv(&encryption_context, iv.data(), iv.size()) ||
                    mbedtls_cipher_set_iv(&decryption_context, iv.data(), iv.size())) == 0,
                   "Failed to set IV on mbedtls ciphers.");
    }

    template <typename Source, typename Dest>
    void Transcode(const Source* src, size_t size, Dest* dest, Op op) {
        size_t written = 0;

        const auto context = op == Op::ENCRYPT ? &encryption_context : &decryption_context;

        mbedtls_cipher_reset(context);

        if (mbedtls_cipher_get_cipher_mode(context) == MBEDTLS_MODE_XTS) {
            mbedtls_cipher_update(context, reinterpret_cast<const u8*>(src), size,
                                  reinterpret_cast<u8*>(dest), &written);
            if (written != size)
                LOG_WARNING(Crypto, "Not all data was decrypted requested={:016X}, actual={:016X}.",
                            size, written);
        } else {
            const auto block_size = mbedtls_cipher_get_block_size(context);

            for (size_t offset = 0; offset < size; offset += block_size) {
                auto length = std::min<size_t>(block_size, size - offset);
                mbedtls_cipher_update(context, reinterpret_cast<const u8*>(src) + offset, length,
                                      reinterpret_cast<u8*>(dest) + offset, &written);
                if (written != length)
                    LOG_WARNING(Crypto,
                                "Not all data was decrypted requested={:016X}, actual={:016X}.",
                                length, written);
            }
        }

        mbedtls_cipher_finish(context, nullptr, nullptr);
    }

    template <typename Source, typename Dest>
    void XTSTranscode(const Source* src, size_t size, Dest* dest, size_t sector_id,
                      size_t sector_size, Op op) {
        if (size % sector_size > 0) {
            LOG_CRITICAL(Crypto, "Data size must be a multiple of sector size.");
            return;
        }

        for (size_t i = 0; i < size; i += sector_size) {
            SetIV(CalculateNintendoTweak(sector_id++));
            Transcode<u8, u8>(reinterpret_cast<const u8*>(src) + i, sector_size,
                              reinterpret_cast<u8*>(dest) + i, op);
        }
    }

private:
    mbedtls_cipher_context_t encryption_context;
    mbedtls_cipher_context_t decryption_context;

    static std::vector<u8> CalculateNintendoTweak(size_t sector_id) {
        std::vector<u8> out(0x10);
        for (size_t i = 0xF; i <= 0xF; --i) {
            out[i] = sector_id & 0xFF;
            sector_id >>= 8;
        }
        return out;
    }
};
} // namespace Crypto
