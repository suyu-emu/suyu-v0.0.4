// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2017 socram8888/amiitool
// SPDX-License-Identifier: MIT

#include <array>
#include <mbedtls/aes.h>
#include <mbedtls/hmac_drbg.h>

#include "common/fs/file.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "core/hle/service/mii/mii_manager.h"
#include "core/hle/service/nfp/amiibo_crypto.h"

namespace Service::NFP::AmiiboCrypto {

bool IsAmiiboValid(const EncryptedNTAG215File& ntag_file) {
    const auto& amiibo_data = ntag_file.user_memory;
    LOG_DEBUG(Service_NFP, "uuid_lock=0x{0:x}", ntag_file.static_lock);
    LOG_DEBUG(Service_NFP, "compability_container=0x{0:x}", ntag_file.compability_container);
    LOG_INFO(Service_NFP, "write_count={}", amiibo_data.write_counter);

    LOG_INFO(Service_NFP, "character_id=0x{0:x}", amiibo_data.model_info.character_id);
    LOG_INFO(Service_NFP, "character_variant={}", amiibo_data.model_info.character_variant);
    LOG_INFO(Service_NFP, "amiibo_type={}", amiibo_data.model_info.amiibo_type);
    LOG_INFO(Service_NFP, "model_number=0x{0:x}", amiibo_data.model_info.model_number);
    LOG_INFO(Service_NFP, "series={}", amiibo_data.model_info.series);
    LOG_DEBUG(Service_NFP, "fixed_value=0x{0:x}", amiibo_data.model_info.constant_value);

    LOG_DEBUG(Service_NFP, "tag_dynamic_lock=0x{0:x}", ntag_file.dynamic_lock);
    LOG_DEBUG(Service_NFP, "tag_CFG0=0x{0:x}", ntag_file.CFG0);
    LOG_DEBUG(Service_NFP, "tag_CFG1=0x{0:x}", ntag_file.CFG1);

    // Validate UUID
    constexpr u8 CT = 0x88; // As defined in `ISO / IEC 14443 - 3`
    if ((CT ^ ntag_file.uuid[0] ^ ntag_file.uuid[1] ^ ntag_file.uuid[2]) != ntag_file.uuid[3]) {
        return false;
    }
    if ((ntag_file.uuid[4] ^ ntag_file.uuid[5] ^ ntag_file.uuid[6] ^ ntag_file.uuid[7]) !=
        ntag_file.uuid[8]) {
        return false;
    }

    // Check against all know constants on an amiibo binary
    if (ntag_file.static_lock != 0xE00F) {
        return false;
    }
    if (ntag_file.compability_container != 0xEEFF10F1U) {
        return false;
    }
    if (amiibo_data.constant_value != 0xA5) {
        return false;
    }
    if (amiibo_data.model_info.constant_value != 0x02) {
        return false;
    }
    // dynamic_lock value apparently is not constant
    // ntag_file.dynamic_lock == 0x0F0001
    if (ntag_file.CFG0 != 0x04000000U) {
        return false;
    }
    if (ntag_file.CFG1 != 0x5F) {
        return false;
    }
    return true;
}

NTAG215File NfcDataToEncodedData(const EncryptedNTAG215File& nfc_data) {
    NTAG215File encoded_data{};

    memcpy(encoded_data.uuid2.data(), nfc_data.uuid.data() + 0x8, 2);
    encoded_data.static_lock = nfc_data.static_lock;
    encoded_data.compability_container = nfc_data.compability_container;
    encoded_data.unfixed_hash = nfc_data.user_memory.unfixed_hash;
    encoded_data.constant_value = nfc_data.user_memory.constant_value;
    encoded_data.write_counter = nfc_data.user_memory.write_counter;
    encoded_data.settings = nfc_data.user_memory.settings;
    encoded_data.owner_mii = nfc_data.user_memory.owner_mii;
    encoded_data.title_id = nfc_data.user_memory.title_id;
    encoded_data.applicaton_write_counter = nfc_data.user_memory.applicaton_write_counter;
    encoded_data.application_area_id = nfc_data.user_memory.application_area_id;
    encoded_data.unknown = nfc_data.user_memory.unknown;
    encoded_data.hash = nfc_data.user_memory.hash;
    encoded_data.application_area = nfc_data.user_memory.application_area;
    encoded_data.locked_hash = nfc_data.user_memory.locked_hash;
    memcpy(encoded_data.uuid.data(), nfc_data.uuid.data(), 8);
    encoded_data.model_info = nfc_data.user_memory.model_info;
    encoded_data.keygen_salt = nfc_data.user_memory.keygen_salt;
    encoded_data.dynamic_lock = nfc_data.dynamic_lock;
    encoded_data.CFG0 = nfc_data.CFG0;
    encoded_data.CFG1 = nfc_data.CFG1;
    encoded_data.password = nfc_data.password;

    return encoded_data;
}

EncryptedNTAG215File EncodedDataToNfcData(const NTAG215File& encoded_data) {
    EncryptedNTAG215File nfc_data{};

    memcpy(nfc_data.uuid.data() + 0x8, encoded_data.uuid2.data(), 2);
    memcpy(nfc_data.uuid.data(), encoded_data.uuid.data(), 8);
    nfc_data.static_lock = encoded_data.static_lock;
    nfc_data.compability_container = encoded_data.compability_container;
    nfc_data.user_memory.unfixed_hash = encoded_data.unfixed_hash;
    nfc_data.user_memory.constant_value = encoded_data.constant_value;
    nfc_data.user_memory.write_counter = encoded_data.write_counter;
    nfc_data.user_memory.settings = encoded_data.settings;
    nfc_data.user_memory.owner_mii = encoded_data.owner_mii;
    nfc_data.user_memory.title_id = encoded_data.title_id;
    nfc_data.user_memory.applicaton_write_counter = encoded_data.applicaton_write_counter;
    nfc_data.user_memory.application_area_id = encoded_data.application_area_id;
    nfc_data.user_memory.unknown = encoded_data.unknown;
    nfc_data.user_memory.hash = encoded_data.hash;
    nfc_data.user_memory.application_area = encoded_data.application_area;
    nfc_data.user_memory.locked_hash = encoded_data.locked_hash;
    nfc_data.user_memory.model_info = encoded_data.model_info;
    nfc_data.user_memory.keygen_salt = encoded_data.keygen_salt;
    nfc_data.dynamic_lock = encoded_data.dynamic_lock;
    nfc_data.CFG0 = encoded_data.CFG0;
    nfc_data.CFG1 = encoded_data.CFG1;
    nfc_data.password = encoded_data.password;

    return nfc_data;
}

u32 GetTagPassword(const TagUuid& uuid) {
    // Verifiy that the generated password is correct
    u32 password = 0xAA ^ (uuid[1] ^ uuid[3]);
    password &= (0x55 ^ (uuid[2] ^ uuid[4])) << 8;
    password &= (0xAA ^ (uuid[3] ^ uuid[5])) << 16;
    password &= (0x55 ^ (uuid[4] ^ uuid[6])) << 24;
    return password;
}

HashSeed GetSeed(const NTAG215File& data) {
    HashSeed seed{
        .data =
            {
                .magic = data.write_counter,
                .padding = {},
                .uuid1 = {},
                .uuid2 = {},
                .keygen_salt = data.keygen_salt,
            },
    };

    // Copy the first 8 bytes of uuid
    memcpy(seed.data.uuid1.data(), data.uuid.data(), sizeof(seed.data.uuid1));
    memcpy(seed.data.uuid2.data(), data.uuid.data(), sizeof(seed.data.uuid2));

    return seed;
}

void PreGenerateKey(const InternalKey& key, const HashSeed& seed, u8* output,
                    std::size_t& outputLen) {
    std::size_t index = 0;

    // Copy whole type string
    memccpy(output + index, key.type_string.data(), '\0', key.type_string.size());
    index += key.type_string.size();

    // Append (16 - magic_length) from the input seed
    std::size_t seedPart1Len = 16 - key.magic_length;
    memcpy(output + index, &seed, seedPart1Len);
    index += seedPart1Len;

    // Append all bytes from magicBytes
    memcpy(output + index, &key.magic_bytes, key.magic_length);
    index += key.magic_length;

    // Seed 16 bytes at +0x10
    memcpy(output + index, &seed.raw[0x10], 16);
    index += 16;

    // 32 bytes at +0x20 from input seed xored with xor pad
    for (std::size_t i = 0; i < 32; i++)
        output[index + i] = seed.raw[i + 32] ^ key.xor_pad[i];
    index += 32;

    outputLen = index;
}

void CryptoInit(CryptoCtx& ctx, mbedtls_md_context_t& hmac_ctx, const HmacKey& hmac_key,
                const u8* seed, std::size_t seed_size) {

    // Initialize context
    ctx.used = false;
    ctx.counter = 0;
    ctx.buffer_size = sizeof(ctx.counter) + seed_size;
    memcpy(ctx.buffer.data() + sizeof(u16), seed, seed_size);

    // Initialize HMAC context
    mbedtls_md_init(&hmac_ctx);
    mbedtls_md_setup(&hmac_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&hmac_ctx, hmac_key.data(), hmac_key.size());
}

void CryptoStep(CryptoCtx& ctx, mbedtls_md_context_t& hmac_ctx, DrgbOutput& output) {
    // If used at least once, reinitialize the HMAC
    if (ctx.used) {
        mbedtls_md_hmac_reset(&hmac_ctx);
    }

    ctx.used = true;

    // Store counter in big endian, and increment it
    ctx.buffer[0] = static_cast<u8>(ctx.counter >> 8);
    ctx.buffer[1] = static_cast<u8>(ctx.counter >> 0);
    ctx.counter++;

    // Do HMAC magic
    mbedtls_md_hmac_update(&hmac_ctx, reinterpret_cast<const unsigned char*>(ctx.buffer.data()),
                           ctx.buffer_size);
    mbedtls_md_hmac_finish(&hmac_ctx, output.data());
}

DerivedKeys GenerateKey(const InternalKey& key, const NTAG215File& data) {
    constexpr std::size_t OUTPUT_SIZE = 512;
    const auto seed = GetSeed(data);

    // Generate internal seed
    u8 internal_key[OUTPUT_SIZE];
    std::size_t internal_key_lenght = 0;
    PreGenerateKey(key, seed, internal_key, internal_key_lenght);

    // Initialize context
    CryptoCtx ctx{};
    mbedtls_md_context_t hmac_ctx;
    CryptoInit(ctx, hmac_ctx, key.hmac_key, internal_key, internal_key_lenght);

    // Generate derived keys
    DerivedKeys derived_keys{};
    std::array<DrgbOutput, 2> temp{};
    CryptoStep(ctx, hmac_ctx, temp[0]);
    CryptoStep(ctx, hmac_ctx, temp[1]);
    memcpy(&derived_keys, temp.data(), sizeof(DerivedKeys));

    // Cleanup context
    mbedtls_md_free(&hmac_ctx);

    return derived_keys;
}

void Cipher(const DerivedKeys& keys, const NTAG215File& in_data, NTAG215File& out_data) {
    mbedtls_aes_context aes;
    std::size_t nc_off = 0;
    std::array<u8, 0x10> nonce_counter{};
    std::array<u8, 0x10> stream_block{};

    mbedtls_aes_setkey_enc(&aes, keys.aes_key.data(), 128);
    memcpy(nonce_counter.data(), keys.aes_iv.data(), sizeof(nonce_counter));

    std::array<u8, sizeof(NTAG215File)> in_data_byes{};
    std::array<u8, sizeof(NTAG215File)> out_data_bytes{};
    memcpy(in_data_byes.data(), &in_data, sizeof(NTAG215File));
    memcpy(out_data_bytes.data(), &out_data, sizeof(NTAG215File));

    mbedtls_aes_crypt_ctr(&aes, 0x188, &nc_off, nonce_counter.data(), stream_block.data(),
                          in_data_byes.data() + 0x2c, out_data_bytes.data() + 0x2c);

    memcpy(out_data_bytes.data(), in_data_byes.data(), 0x008);
    // Data signature NOT copied
    memcpy(out_data_bytes.data() + 0x028, in_data_byes.data() + 0x028, 0x004);
    // Tag signature NOT copied
    memcpy(out_data_bytes.data() + 0x1D4, in_data_byes.data() + 0x1D4, 0x048);

    memcpy(&out_data, out_data_bytes.data(), sizeof(NTAG215File));
}

bool LoadKeys(InternalKey& locked_secret, InternalKey& unfixed_info) {
    const auto yuzu_keys_dir = Common::FS::GetYuzuPath(Common::FS::YuzuPath::KeysDir);

    const Common::FS::IOFile keys_file{yuzu_keys_dir / "key_retail.bin",
                                       Common::FS::FileAccessMode::Read,
                                       Common::FS::FileType::BinaryFile};

    if (!keys_file.IsOpen()) {
        LOG_ERROR(Service_NFP, "No keys detected");
        return false;
    }

    if (keys_file.Read(unfixed_info) != 1) {
        LOG_ERROR(Service_NFP, "Failed to read unfixed_info");
        return false;
    }
    if (keys_file.Read(locked_secret) != 1) {
        LOG_ERROR(Service_NFP, "Failed to read locked-secret");
        return false;
    }

    return true;
}

bool DecodeAmiibo(const EncryptedNTAG215File& encrypted_tag_data, NTAG215File& tag_data) {
    InternalKey locked_secret{};
    InternalKey unfixed_info{};

    if (!LoadKeys(locked_secret, unfixed_info)) {
        return false;
    }

    // Generate keys
    NTAG215File encoded_data = NfcDataToEncodedData(encrypted_tag_data);
    const auto data_keys = GenerateKey(unfixed_info, encoded_data);
    const auto tag_keys = GenerateKey(locked_secret, encoded_data);

    // Decrypt
    Cipher(data_keys, encoded_data, tag_data);

    std::array<u8, sizeof(NTAG215File)> out{};
    memcpy(out.data(), &tag_data, sizeof(NTAG215File));

    // Regenerate tag HMAC. Note: order matters, data HMAC depends on tag HMAC!
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), tag_keys.hmac_key.data(),
                    sizeof(HmacKey), out.data() + 0x1D4, 0x34, out.data() + HMAC_POS_TAG);

    // Regenerate data HMAC
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), data_keys.hmac_key.data(),
                    sizeof(HmacKey), out.data() + 0x29, 0x1DF, out.data() + HMAC_POS_DATA);

    memcpy(&tag_data, out.data(), sizeof(NTAG215File));

    if (memcmp(tag_data.unfixed_hash.data(), encrypted_tag_data.user_memory.unfixed_hash.data(),
               32) != 0) {
        return false;
    }

    if (memcmp(tag_data.locked_hash.data(), encrypted_tag_data.user_memory.locked_hash.data(),
               32) != 0) {
        return false;
    }

    return true;
}

bool EncodeAmiibo(const NTAG215File& tag_data, EncryptedNTAG215File& encrypted_tag_data) {
    InternalKey locked_secret{};
    InternalKey unfixed_info{};

    if (!LoadKeys(locked_secret, unfixed_info)) {
        return false;
    }

    // Generate keys
    const auto data_keys = GenerateKey(unfixed_info, tag_data);
    const auto tag_keys = GenerateKey(locked_secret, tag_data);

    std::array<u8, sizeof(NTAG215File)> plain{};
    std::array<u8, sizeof(NTAG215File)> cipher{};
    memcpy(plain.data(), &tag_data, sizeof(NTAG215File));

    // Generate tag HMAC
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), tag_keys.hmac_key.data(),
                    sizeof(HmacKey), plain.data() + 0x1D4, 0x34, cipher.data() + HMAC_POS_TAG);

    // Init mbedtls HMAC context
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);

    // Generate data HMAC
    mbedtls_md_hmac_starts(&ctx, data_keys.hmac_key.data(), sizeof(HmacKey));
    mbedtls_md_hmac_update(&ctx, plain.data() + 0x029, 0x18B);        // Data
    mbedtls_md_hmac_update(&ctx, cipher.data() + HMAC_POS_TAG, 0x20); // Tag HMAC
    mbedtls_md_hmac_update(&ctx, plain.data() + 0x1D4, 0x34);
    mbedtls_md_hmac_finish(&ctx, cipher.data() + HMAC_POS_DATA);

    // HMAC cleanup
    mbedtls_md_free(&ctx);

    // Encrypt
    NTAG215File encoded_tag_data{};
    memcpy(&encoded_tag_data, cipher.data(), sizeof(NTAG215File));
    Cipher(data_keys, tag_data, encoded_tag_data);

    // Convert back to hardware
    encrypted_tag_data = EncodedDataToNfcData(encoded_tag_data);

    return true;
}

} // namespace Service::NFP::AmiiboCrypto
