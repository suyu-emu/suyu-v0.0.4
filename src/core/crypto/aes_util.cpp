// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/crypto/aes_util.h"
#include "mbedtls/cipher.h"

namespace Core::Crypto {
static_assert(static_cast<size_t>(Mode::CTR) == static_cast<size_t>(MBEDTLS_CIPHER_AES_128_CTR), "CTR mode is incorrect.");
static_assert(static_cast<size_t>(Mode::ECB) == static_cast<size_t>(MBEDTLS_CIPHER_AES_128_ECB), "ECB mode is incorrect.");
static_assert(static_cast<size_t>(Mode::XTS) == static_cast<size_t>(MBEDTLS_CIPHER_AES_128_XTS), "XTS mode is incorrect.");

template<typename Key, size_t KeySize>
Crypto::AESCipher<Key, KeySize>::AESCipher(Key key, Mode mode) {
    mbedtls_cipher_init(encryption_context.get());
    mbedtls_cipher_init(decryption_context.get());

    ASSERT_MSG((mbedtls_cipher_setup(
            encryption_context.get(),
            mbedtls_cipher_info_from_type(static_cast<mbedtls_cipher_type_t>(mode))) ||
                mbedtls_cipher_setup(decryption_context.get(),
                                     mbedtls_cipher_info_from_type(
                                             static_cast<mbedtls_cipher_type_t>(mode)))) == 0,
               "Failed to initialize mbedtls ciphers.");

    ASSERT(
            !mbedtls_cipher_setkey(encryption_context.get(), key.data(), KeySize * 8, MBEDTLS_ENCRYPT));
    ASSERT(
            !mbedtls_cipher_setkey(decryption_context.get(), key.data(), KeySize * 8, MBEDTLS_DECRYPT));
    //"Failed to set key on mbedtls ciphers.");
}

template<typename Key, size_t KeySize>
AESCipher<Key, KeySize>::~AESCipher() {
    mbedtls_cipher_free(encryption_context.get());
    mbedtls_cipher_free(decryption_context.get());
}

template<typename Key, size_t KeySize>
void AESCipher<Key, KeySize>::SetIV(std::vector<u8> iv) {
    ASSERT_MSG((mbedtls_cipher_set_iv(encryption_context.get(), iv.data(), iv.size()) ||
                mbedtls_cipher_set_iv(decryption_context.get(), iv.data(), iv.size())) == 0,
               "Failed to set IV on mbedtls ciphers.");
}

template<typename Key, size_t KeySize>
void AESCipher<Key, KeySize>::Transcode(const u8* src, size_t size, u8* dest, Op op)  {
    size_t written = 0;

    const auto context = op == Op::Encrypt ? encryption_context.get() : decryption_context.get();

    mbedtls_cipher_reset(context);

    if (mbedtls_cipher_get_cipher_mode(context) == MBEDTLS_MODE_XTS) {
        mbedtls_cipher_update(context, src, size,
                              dest, &written);
        if (written != size)
            LOG_WARNING(Crypto, "Not all data was decrypted requested={:016X}, actual={:016X}.",
                        size, written);
    } else {
        const auto block_size = mbedtls_cipher_get_block_size(context);

        for (size_t offset = 0; offset < size; offset += block_size) {
            auto length = std::min<size_t>(block_size, size - offset);
            mbedtls_cipher_update(context, src + offset, length,
                                  dest + offset, &written);
            if (written != length)
                LOG_WARNING(Crypto,
                            "Not all data was decrypted requested={:016X}, actual={:016X}.",
                            length, written);
        }
    }

    mbedtls_cipher_finish(context, nullptr, nullptr);
}

template<typename Key, size_t KeySize>
void AESCipher<Key, KeySize>::XTSTranscode(const u8* src, size_t size, u8* dest, size_t sector_id, size_t sector_size,
                                           Op op) {
    if (size % sector_size > 0) {
        LOG_CRITICAL(Crypto, "Data size must be a multiple of sector size.");
        return;
    }

    for (size_t i = 0; i < size; i += sector_size) {
        SetIV(CalculateNintendoTweak(sector_id++));
        Transcode<u8, u8>(src + i, sector_size,
                          dest + i, op);
    }
}

template<typename Key, size_t KeySize>
std::vector<u8> AESCipher<Key, KeySize>::CalculateNintendoTweak(size_t sector_id) {
    std::vector<u8> out(0x10);
    for (size_t i = 0xF; i <= 0xF; --i) {
        out[i] = sector_id & 0xFF;
        sector_id >>= 8;
    }
    return out;
}

template class AESCipher<Key128>;
template class AESCipher<Key256>;
}