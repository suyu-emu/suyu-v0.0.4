// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstring>
#include <openssl/err.h>
#include <openssl/evp.h>
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

// Structure to hide OpenSSL types from header file
struct CipherContext {
    EVP_CIPHER_CTX* encryption_context = nullptr;
    EVP_CIPHER_CTX* decryption_context = nullptr;
    EVP_CIPHER* cipher = nullptr;
};

static inline const std::string GetCipherName(Mode mode, u32 key_size) {
    std::string cipher;
    std::size_t effective_bits = key_size * 8;

    switch (mode) {
    case Mode::CTR:
        cipher = "CTR";
        break;
    case Mode::ECB:
        cipher = "ECB";
        break;
    case Mode::XTS:
        cipher = "XTS";
        effective_bits /= 2;
        break;
    default:
        UNREACHABLE();
    }

    return fmt::format("AES-{}-{}", effective_bits, cipher);
};

static EVP_CIPHER *GetCipher(Mode mode, u32 key_size) {
    static auto fetch_cipher = [](Mode m, u32 k) {
        return EVP_CIPHER_fetch(nullptr, GetCipherName(m, k).c_str(), nullptr);
    };

    static const struct {
        EVP_CIPHER* ctr_16 = fetch_cipher(Mode::CTR, 16);
        EVP_CIPHER* ecb_16 = fetch_cipher(Mode::ECB, 16);
        EVP_CIPHER* xts_16 = fetch_cipher(Mode::XTS, 16);
        EVP_CIPHER* ctr_32 = fetch_cipher(Mode::CTR, 32);
        EVP_CIPHER* ecb_32 = fetch_cipher(Mode::ECB, 32);
        EVP_CIPHER* xts_32 = fetch_cipher(Mode::XTS, 32);
    } ciphers = {};

    switch (mode) {
    case Mode::CTR:
        return key_size == 16 ? ciphers.ctr_16 : ciphers.ctr_32;
    case Mode::ECB:
        return key_size == 16 ? ciphers.ecb_16 : ciphers.ecb_32;
    case Mode::XTS:
        return key_size == 16 ? ciphers.xts_16 : ciphers.xts_32;
    default:
        UNIMPLEMENTED();
    }

    return nullptr;
}

// TODO: WHY TEMPLATE???????
template <typename Key, std::size_t KeySize>
Crypto::AESCipher<Key, KeySize>::AESCipher(Key key, Mode mode)
    : ctx(std::make_unique<CipherContext>()) {

    ctx->encryption_context = EVP_CIPHER_CTX_new();
    ctx->decryption_context = EVP_CIPHER_CTX_new();
    ctx->cipher = GetCipher(mode, KeySize);
    if (ctx->cipher) {
        EVP_CIPHER_up_ref(ctx->cipher);
    } else {
        UNIMPLEMENTED();
    }

    ASSERT_MSG(ctx->encryption_context && ctx->decryption_context && ctx->cipher,
               "OpenSSL cipher context failed init!");

    // now init ciphers
    ASSERT(EVP_CipherInit_ex2(ctx->encryption_context, ctx->cipher, key.data(), NULL, 1, NULL));
    ASSERT(EVP_CipherInit_ex2(ctx->decryption_context, ctx->cipher, key.data(), NULL, 0, NULL));

    EVP_CIPHER_CTX_set_padding(ctx->encryption_context, 0);
    EVP_CIPHER_CTX_set_padding(ctx->decryption_context, 0);
}

template <typename Key, std::size_t KeySize>
AESCipher<Key, KeySize>::~AESCipher() {
    EVP_CIPHER_CTX_free(ctx->encryption_context);
    EVP_CIPHER_CTX_free(ctx->decryption_context);
    EVP_CIPHER_free(ctx->cipher);
}

template <typename Key, std::size_t KeySize>
void AESCipher<Key, KeySize>::Transcode(const u8* src, std::size_t size, u8* dest, Op op) const {
    auto* const context = op == Op::Encrypt ? ctx->encryption_context : ctx->decryption_context;

    if (size == 0)
        return;

    // reset
    ASSERT(EVP_CipherInit_ex(context, nullptr, nullptr, nullptr, nullptr, -1));

    const int block_size = EVP_CIPHER_CTX_get_block_size(context);
    ASSERT(block_size > 0 && block_size <= int(AesBlockBytes));

    const std::size_t whole_block_bytes = size - (size % block_size);
    int written = 0;

    if (whole_block_bytes != 0) {
        ASSERT(EVP_CipherUpdate(context, dest, &written, src, static_cast<int>(whole_block_bytes)));

        if (std::size_t(written) != whole_block_bytes) {
            LOG_WARNING(Crypto, "Not all data was processed requested={:016X}, actual={:016X}.",
                        whole_block_bytes, written);
        }
    }

    // tail
    const std::size_t tail = size - whole_block_bytes;
    if (tail == 0)
        return;

    std::array<u8, AesBlockBytes> tail_buffer{};
    std::memcpy(tail_buffer.data(), src + whole_block_bytes, tail);

    int tail_written = 0;

    ASSERT(EVP_CipherUpdate(context, tail_buffer.data(), &tail_written, tail_buffer.data(), block_size));

    if (tail_written != block_size) {
        LOG_WARNING(Crypto, "Tail block not fully processed requested={:016X}, actual={:016X}.",
                    block_size, tail_written);
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
    const int ret_enc = EVP_CipherInit_ex(ctx->encryption_context, nullptr, nullptr, nullptr, data.data(), -1);
    const int ret_dec = EVP_CipherInit_ex(ctx->decryption_context, nullptr, nullptr, nullptr, data.data(), -1);

    ASSERT_MSG(ret_enc == 1 && ret_dec == 1, "Failed to set IV on OpenSSL contexts");
}

template class AESCipher<Key128>;
template class AESCipher<Key256>;
} // namespace Core::Crypto
