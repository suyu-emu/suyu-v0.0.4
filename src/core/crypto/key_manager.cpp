// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <fstream>
#include <locale>
#include <sstream>
#include <string_view>
#include <tuple>
#include <vector>
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/hex_util.h"
#include "common/logging/log.h"
#include "core/crypto/aes_util.h"
#include "core/crypto/key_manager.h"
#include "core/loader/loader.h"
#include "core/settings.h"

namespace Core::Crypto {

constexpr u64 CURRENT_CRYPTO_REVISION = 0x5;

Key128 GenerateKeyEncryptionKey(Key128 source, Key128 master, Key128 kek_seed, Key128 key_seed) {
    Key128 out{};

    AESCipher<Key128> cipher1(master, Mode::ECB);
    cipher1.Transcode(kek_seed.data(), kek_seed.size(), out.data(), Op::Decrypt);
    AESCipher<Key128> cipher2(out, Mode::ECB);
    cipher2.Transcode(source.data(), source.size(), out.data(), Op::Decrypt);

    if (key_seed != Key128{}) {
        AESCipher<Key128> cipher3(out, Mode::ECB);
        cipher3.Transcode(key_seed.data(), key_seed.size(), out.data(), Op::Decrypt);
    }

    return out;
}

Key128 DeriveKeyblobKey(Key128 sbk, Key128 tsec, Key128 source) {
    AESCipher<Key128> sbk_cipher(sbk, Mode::ECB);
    AESCipher<Key128> tsec_cipher(tsec, Mode::ECB);
    tsec_cipher.Transcode(source.data(), source.size(), source.data(), Op::Decrypt);
    sbk_cipher.Transcode(source.data(), source.size(), source.data(), Op::Decrypt);
    return source;
}

boost::optional<Key128> DeriveSDSeed() {
    const FileUtil::IOFile save_43(FileUtil::GetUserPath(FileUtil::UserPath::NANDDir) +
                                       "/system/save/8000000000000043",
                                   "rb+");
    if (!save_43.IsOpen())
        return boost::none;
    const FileUtil::IOFile sd_private(
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir) + "/Nintendo/Contents/private", "rb+");
    if (!sd_private.IsOpen())
        return boost::none;

    sd_private.Seek(0, SEEK_SET);
    std::array<u8, 0x10> private_seed{};
    if (sd_private.ReadBytes(private_seed.data(), private_seed.size()) != 0x10)
        return boost::none;

    std::array<u8, 0x10> buffer{};
    std::size_t offset = 0;
    for (; offset + 0x10 < save_43.GetSize(); ++offset) {
        save_43.Seek(offset, SEEK_SET);
        save_43.ReadBytes(buffer.data(), buffer.size());
        if (buffer == private_seed)
            break;
    }

    if (offset + 0x10 >= save_43.GetSize())
        return boost::none;

    Key128 seed{};
    save_43.Seek(offset + 0x10, SEEK_SET);
    save_43.ReadBytes(seed.data(), seed.size());
    return seed;
}

Loader::ResultStatus DeriveSDKeys(std::array<Key256, 2>& sd_keys, KeyManager& keys) {
    if (!keys.HasKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::SDKek)))
        return Loader::ResultStatus::ErrorMissingSDKEKSource;
    if (!keys.HasKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::AESKekGeneration)))
        return Loader::ResultStatus::ErrorMissingAESKEKGenerationSource;
    if (!keys.HasKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::AESKeyGeneration)))
        return Loader::ResultStatus::ErrorMissingAESKeyGenerationSource;

    const auto sd_kek_source =
        keys.GetKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::SDKek));
    const auto aes_kek_gen =
        keys.GetKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::AESKekGeneration));
    const auto aes_key_gen =
        keys.GetKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::AESKeyGeneration));
    const auto master_00 = keys.GetKey(S128KeyType::Master);
    const auto sd_kek =
        GenerateKeyEncryptionKey(sd_kek_source, master_00, aes_kek_gen, aes_key_gen);
    keys.SetKey(S128KeyType::SDKek, sd_kek);

    if (!keys.HasKey(S128KeyType::SDSeed))
        return Loader::ResultStatus::ErrorMissingSDSeed;
    const auto sd_seed = keys.GetKey(S128KeyType::SDSeed);

    if (!keys.HasKey(S256KeyType::SDKeySource, static_cast<u64>(SDKeyType::Save)))
        return Loader::ResultStatus::ErrorMissingSDSaveKeySource;
    if (!keys.HasKey(S256KeyType::SDKeySource, static_cast<u64>(SDKeyType::NCA)))
        return Loader::ResultStatus::ErrorMissingSDNCAKeySource;

    std::array<Key256, 2> sd_key_sources{
        keys.GetKey(S256KeyType::SDKeySource, static_cast<u64>(SDKeyType::Save)),
        keys.GetKey(S256KeyType::SDKeySource, static_cast<u64>(SDKeyType::NCA)),
    };

    // Combine sources and seed
    for (auto& source : sd_key_sources) {
        for (std::size_t i = 0; i < source.size(); ++i)
            source[i] ^= sd_seed[i & 0xF];
    }

    AESCipher<Key128> cipher(sd_kek, Mode::ECB);
    // The transform manipulates sd_keys as part of the Transcode, so the return/output is
    // unnecessary. This does not alter sd_keys_sources.
    std::transform(sd_key_sources.begin(), sd_key_sources.end(), sd_keys.begin(),
                   sd_key_sources.begin(), [&cipher](const Key256& source, Key256& out) {
                       cipher.Transcode(source.data(), source.size(), out.data(), Op::Decrypt);
                       return source; ///< Return unaltered source to satisfy output requirement.
                   });

    return Loader::ResultStatus::Success;
}

KeyManager::KeyManager() {
    // Initialize keys
    const std::string hactool_keys_dir = FileUtil::GetHactoolConfigurationPath();
    const std::string yuzu_keys_dir = FileUtil::GetUserPath(FileUtil::UserPath::KeysDir);
    if (Settings::values.use_dev_keys) {
        dev_mode = true;
        AttemptLoadKeyFile(yuzu_keys_dir, hactool_keys_dir, "dev.keys", false);
        AttemptLoadKeyFile(yuzu_keys_dir, yuzu_keys_dir, "dev.keys_autogenerated", false);
    } else {
        dev_mode = false;
        AttemptLoadKeyFile(yuzu_keys_dir, hactool_keys_dir, "prod.keys", false);
        AttemptLoadKeyFile(yuzu_keys_dir, yuzu_keys_dir, "prod.keys_autogenerated", false);
    }

    AttemptLoadKeyFile(yuzu_keys_dir, hactool_keys_dir, "title.keys", true);
    AttemptLoadKeyFile(yuzu_keys_dir, yuzu_keys_dir, "title.keys_autogenerated", true);
    AttemptLoadKeyFile(yuzu_keys_dir, hactool_keys_dir, "console.keys", false);
    AttemptLoadKeyFile(yuzu_keys_dir, yuzu_keys_dir, "console.keys_autogenerated", false);
}

static bool ValidCryptoRevisionString(const std::string& base, size_t begin, size_t length) {
    if (base.size() < begin + length)
        return false;
    return std::all_of(base.begin() + begin, base.begin() + begin + length, ::isdigit);
}

void KeyManager::LoadFromFile(const std::string& filename, bool is_title_keys) {
    std::ifstream file(filename);
    if (!file.is_open())
        return;

    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> out;
        std::stringstream stream(line);
        std::string item;
        while (std::getline(stream, item, '='))
            out.push_back(std::move(item));

        if (out.size() != 2)
            continue;

        out[0].erase(std::remove(out[0].begin(), out[0].end(), ' '), out[0].end());
        out[1].erase(std::remove(out[1].begin(), out[1].end(), ' '), out[1].end());

        if (out[0].compare(0, 1, "#") == 0)
            continue;

        if (is_title_keys) {
            auto rights_id_raw = Common::HexStringToArray<16>(out[0]);
            u128 rights_id{};
            std::memcpy(rights_id.data(), rights_id_raw.data(), rights_id_raw.size());
            Key128 key = Common::HexStringToArray<16>(out[1]);
            s128_keys[{S128KeyType::Titlekey, rights_id[1], rights_id[0]}] = key;
        } else {
            std::transform(out[0].begin(), out[0].end(), out[0].begin(), ::tolower);
            if (s128_file_id.find(out[0]) != s128_file_id.end()) {
                const auto index = s128_file_id.at(out[0]);
                Key128 key = Common::HexStringToArray<16>(out[1]);
                s128_keys[{index.type, index.field1, index.field2}] = key;
            } else if (s256_file_id.find(out[0]) != s256_file_id.end()) {
                const auto index = s256_file_id.at(out[0]);
                Key256 key = Common::HexStringToArray<32>(out[1]);
                s256_keys[{index.type, index.field1, index.field2}] = key;
            } else if (out[0].compare(0, 8, "keyblob_") == 0 &&
                       out[0].compare(0, 9, "keyblob_k") != 0) {
                if (!ValidCryptoRevisionString(out[0], 8, 2))
                    continue;

                const auto index = std::stoul(out[0].substr(8, 2), nullptr, 16);
                keyblobs[index] = Common::HexStringToArray<0x90>(out[1]);
            } else if (out[0].compare(0, 18, "encrypted_keyblob_") == 0) {
                if (!ValidCryptoRevisionString(out[0], 18, 2))
                    continue;

                const auto index = std::stoul(out[0].substr(18, 2), nullptr, 16);
                encrypted_keyblobs[index] = Common::HexStringToArray<0xB0>(out[1]);
            } else {
                for (const auto& kv : std::map<std::pair<S128KeyType, u64>, std::string>{
                         {{S128KeyType::Master, 0}, "master_key_"},
                         {{S128KeyType::Package1, 0}, "package1_key_"},
                         {{S128KeyType::Package2, 0}, "package2_key_"},
                         {{S128KeyType::Titlekek, 0}, "titlekek_"},
                         {{S128KeyType::Source, static_cast<u64>(SourceKeyType::Keyblob)},
                          "keyblob_key_source_"},
                         {{S128KeyType::Keyblob, 0}, "keyblob_key_"},
                         {{S128KeyType::KeyblobMAC, 0}, "keyblob_mac_key_"},
                     }) {
                    if (!ValidCryptoRevisionString(out[0], kv.second.size(), 2))
                        continue;
                    if (out[0].compare(0, kv.second.size(), kv.second) == 0) {
                        const auto index =
                            std::stoul(out[0].substr(kv.second.size(), 2), nullptr, 16);
                        const auto sub = kv.first.second;
                        if (sub == 0) {
                            s128_keys[{kv.first.first, index, 0}] =
                                Common::HexStringToArray<16>(out[1]);
                        } else {
                            s128_keys[{kv.first.first, kv.first.second, index}] =
                                Common::HexStringToArray<16>(out[1]);
                        }

                        break;
                    }
                }

                const static std::array<const char*, 3> kak_names = {
                    "key_area_key_application_", "key_area_key_ocean_", "key_area_key_system_"};
                for (size_t j = 0; j < 3; ++j) {
                    const auto& match = kak_names[j];
                    if (out[0].compare(0, std::strlen(match), match) == 0) {
                        const auto index =
                            std::stoul(out[0].substr(std::strlen(match), 2), nullptr, 16);
                        s128_keys[{S128KeyType::KeyArea, index, j}] =
                            Common::HexStringToArray<16>(out[1]);
                    }
                }
            }
        }
    }
}

void KeyManager::AttemptLoadKeyFile(const std::string& dir1, const std::string& dir2,
                                    const std::string& filename, bool title) {
    if (FileUtil::Exists(dir1 + DIR_SEP + filename))
        LoadFromFile(dir1 + DIR_SEP + filename, title);
    else if (FileUtil::Exists(dir2 + DIR_SEP + filename))
        LoadFromFile(dir2 + DIR_SEP + filename, title);
}

bool KeyManager::BaseDeriveNecessary() {
    const auto check_key_existence = [this](auto key_type, u64 index1 = 0, u64 index2 = 0) {
        return !HasKey(key_type, index1, index2);
    };

    if (check_key_existence(S256KeyType::Header))
        return true;

    for (size_t i = 0; i < CURRENT_CRYPTO_REVISION; ++i) {
        if (check_key_existence(S128KeyType::Master, i) ||
            check_key_existence(S128KeyType::KeyArea, i,
                                static_cast<u64>(KeyAreaKeyType::Application)) ||
            check_key_existence(S128KeyType::KeyArea, i, static_cast<u64>(KeyAreaKeyType::Ocean)) ||
            check_key_existence(S128KeyType::KeyArea, i,
                                static_cast<u64>(KeyAreaKeyType::System)) ||
            check_key_existence(S128KeyType::Titlekek, i))
            return true;
    }

    return false;
}

bool KeyManager::HasKey(S128KeyType id, u64 field1, u64 field2) const {
    return s128_keys.find({id, field1, field2}) != s128_keys.end();
}

bool KeyManager::HasKey(S256KeyType id, u64 field1, u64 field2) const {
    return s256_keys.find({id, field1, field2}) != s256_keys.end();
}

Key128 KeyManager::GetKey(S128KeyType id, u64 field1, u64 field2) const {
    if (!HasKey(id, field1, field2))
        return {};
    return s128_keys.at({id, field1, field2});
}

Key256 KeyManager::GetKey(S256KeyType id, u64 field1, u64 field2) const {
    if (!HasKey(id, field1, field2))
        return {};
    return s256_keys.at({id, field1, field2});
}

Key256 KeyManager::GetBISKey(u8 partition_id) const {
    Key256 out{};

    for (const auto& bis_type : {BISKeyType::Crypto, BISKeyType::Tweak}) {
        if (HasKey(S128KeyType::BIS, partition_id, static_cast<u64>(bis_type))) {
            std::memcpy(
                out.data() + sizeof(Key128) * static_cast<u64>(bis_type),
                s128_keys.at({S128KeyType::BIS, partition_id, static_cast<u64>(bis_type)}).data(),
                sizeof(Key128));
        }
    }

    return out;
}

template <size_t Size>
void KeyManager::WriteKeyToFile(KeyCategory category, std::string_view keyname,
                                const std::array<u8, Size>& key) {
    const std::string yuzu_keys_dir = FileUtil::GetUserPath(FileUtil::UserPath::KeysDir);
    std::string filename = "title.keys_autogenerated";
    if (category == KeyCategory::Standard)
        filename = dev_mode ? "dev.keys_autogenerated" : "prod.keys_autogenerated";
    else if (category == KeyCategory::Console)
        filename = "console.keys_autogenerated";
    const auto add_info_text = !FileUtil::Exists(yuzu_keys_dir + DIR_SEP + filename);
    FileUtil::CreateFullPath(yuzu_keys_dir + DIR_SEP + filename);
    std::ofstream file(yuzu_keys_dir + DIR_SEP + filename, std::ios::app);
    if (!file.is_open())
        return;
    if (add_info_text) {
        file
            << "# This file is autogenerated by Yuzu\n"
            << "# It serves to store keys that were automatically generated from the normal keys\n"
            << "# If you are experiencing issues involving keys, it may help to delete this file\n";
    }

    file << fmt::format("\n{} = {}", keyname, Common::HexArrayToString(key));
    AttemptLoadKeyFile(yuzu_keys_dir, yuzu_keys_dir, filename, category == KeyCategory::Title);
}

void KeyManager::SetKey(S128KeyType id, Key128 key, u64 field1, u64 field2) {
    if (s128_keys.find({id, field1, field2}) != s128_keys.end())
        return;
    if (id == S128KeyType::Titlekey) {
        Key128 rights_id;
        std::memcpy(rights_id.data(), &field2, sizeof(u64));
        std::memcpy(rights_id.data() + sizeof(u64), &field1, sizeof(u64));
        WriteKeyToFile(KeyCategory::Title, Common::HexArrayToString(rights_id), key);
    }

    auto category = KeyCategory::Standard;
    if (id == S128KeyType::Keyblob || id == S128KeyType::KeyblobMAC || id == S128KeyType::TSEC ||
        id == S128KeyType::SecureBoot || id == S128KeyType::SDSeed || id == S128KeyType::BIS) {
        category = KeyCategory::Console;
    }

    const auto iter2 = std::find_if(
        s128_file_id.begin(), s128_file_id.end(),
        [&id, &field1, &field2](const std::pair<std::string, KeyIndex<S128KeyType>> elem) {
            return std::tie(elem.second.type, elem.second.field1, elem.second.field2) ==
                   std::tie(id, field1, field2);
        });
    if (iter2 != s128_file_id.end())
        WriteKeyToFile(category, iter2->first, key);

    // Variable cases
    if (id == S128KeyType::KeyArea) {
        const static std::array<const char*, 3> kak_names = {"key_area_key_application_{:02X}",
                                                             "key_area_key_ocean_{:02X}",
                                                             "key_area_key_system_{:02X}"};
        WriteKeyToFile(category, fmt::format(kak_names.at(field2), field1), key);
    } else if (id == S128KeyType::Master) {
        WriteKeyToFile(category, fmt::format("master_key_{:02X}", field1), key);
    } else if (id == S128KeyType::Package1) {
        WriteKeyToFile(category, fmt::format("package1_key_{:02X}", field1), key);
    } else if (id == S128KeyType::Package2) {
        WriteKeyToFile(category, fmt::format("package2_key_{:02X}", field1), key);
    } else if (id == S128KeyType::Titlekek) {
        WriteKeyToFile(category, fmt::format("titlekek_{:02X}", field1), key);
    } else if (id == S128KeyType::Keyblob) {
        WriteKeyToFile(category, fmt::format("keyblob_key_{:02X}", field1), key);
    } else if (id == S128KeyType::KeyblobMAC) {
        WriteKeyToFile(category, fmt::format("keyblob_mac_key_{:02X}", field1), key);
    } else if (id == S128KeyType::Source && field1 == static_cast<u64>(SourceKeyType::Keyblob)) {
        WriteKeyToFile(category, fmt::format("keyblob_key_source_{:02X}", field2), key);
    }

    s128_keys[{id, field1, field2}] = key;
}

void KeyManager::SetKey(S256KeyType id, Key256 key, u64 field1, u64 field2) {
    if (s256_keys.find({id, field1, field2}) != s256_keys.end())
        return;
    const auto iter = std::find_if(
        s256_file_id.begin(), s256_file_id.end(),
        [&id, &field1, &field2](const std::pair<std::string, KeyIndex<S256KeyType>> elem) {
            return std::tie(elem.second.type, elem.second.field1, elem.second.field2) ==
                   std::tie(id, field1, field2);
        });
    if (iter != s256_file_id.end())
        WriteKeyToFile(KeyCategory::Standard, iter->first, key);
    s256_keys[{id, field1, field2}] = key;
}

bool KeyManager::KeyFileExists(bool title) {
    const std::string hactool_keys_dir = FileUtil::GetHactoolConfigurationPath();
    const std::string yuzu_keys_dir = FileUtil::GetUserPath(FileUtil::UserPath::KeysDir);
    if (title) {
        return FileUtil::Exists(hactool_keys_dir + DIR_SEP + "title.keys") ||
               FileUtil::Exists(yuzu_keys_dir + DIR_SEP + "title.keys");
    }

    if (Settings::values.use_dev_keys) {
        return FileUtil::Exists(hactool_keys_dir + DIR_SEP + "dev.keys") ||
               FileUtil::Exists(yuzu_keys_dir + DIR_SEP + "dev.keys");
    }

    return FileUtil::Exists(hactool_keys_dir + DIR_SEP + "prod.keys") ||
           FileUtil::Exists(yuzu_keys_dir + DIR_SEP + "prod.keys");
}

void KeyManager::DeriveSDSeedLazy() {
    if (HasKey(S128KeyType::SDSeed))
        return;

    const auto res = DeriveSDSeed();
    if (res != boost::none)
        SetKey(S128KeyType::SDSeed, res.get());
}

static Key128 CalculateCMAC(const u8* source, size_t size, Key128 key) {
    Key128 out{};

    mbedtls_cipher_cmac(mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB), key.data(), 0x80,
                        source, size, out.data());
    return out;
}

void KeyManager::DeriveBase() {
    if (!BaseDeriveNecessary())
        return;

    if (!HasKey(S128KeyType::SecureBoot) || !HasKey(S128KeyType::TSEC))
        return;

    const auto has_bis = [this](u64 id) {
        return HasKey(S128KeyType::BIS, id, static_cast<u64>(BISKeyType::Crypto)) &&
               HasKey(S128KeyType::BIS, id, static_cast<u64>(BISKeyType::Tweak));
    };

    const auto copy_bis = [this](u64 id_from, u64 id_to) {
        SetKey(S128KeyType::BIS,
               GetKey(S128KeyType::BIS, id_from, static_cast<u64>(BISKeyType::Crypto)), id_to,
               static_cast<u64>(BISKeyType::Crypto));

        SetKey(S128KeyType::BIS,
               GetKey(S128KeyType::BIS, id_from, static_cast<u64>(BISKeyType::Tweak)), id_to,
               static_cast<u64>(BISKeyType::Tweak));
    };

    if (has_bis(2) && !has_bis(3))
        copy_bis(2, 3);
    else if (has_bis(3) && !has_bis(2))
        copy_bis(3, 2);

    std::bitset<32> revisions{};
    revisions.set();
    for (size_t i = 0; i < 32; ++i) {
        if (!HasKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::Keyblob), i) ||
            encrypted_keyblobs[i] == std::array<u8, 0xB0>{})
            revisions.reset(i);
    }

    if (!revisions.any())
        return;

    const auto sbk = GetKey(S128KeyType::SecureBoot);
    const auto tsec = GetKey(S128KeyType::TSEC);
    const auto master_source = GetKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::Master));
    const auto kek_generation_source =
        GetKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::AESKekGeneration));
    const auto key_generation_source =
        GetKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::AESKeyGeneration));

    for (size_t i = 0; i < 32; ++i) {
        if (!revisions[i])
            continue;

        // Derive keyblob key
        const auto key = DeriveKeyblobKey(
            sbk, tsec, GetKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::Keyblob), i));

        SetKey(S128KeyType::Keyblob, key, i);

        // Derive keyblob MAC key
        if (!HasKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::KeyblobMAC)))
            continue;

        const auto mac_source =
            GetKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::KeyblobMAC));

        AESCipher<Key128> mac_cipher(key, Mode::ECB);
        Key128 mac_key{};
        mac_cipher.Transcode(mac_source.data(), mac_key.size(), mac_key.data(), Op::Decrypt);

        SetKey(S128KeyType::KeyblobMAC, mac_key, i);

        Key128 cmac = CalculateCMAC(encrypted_keyblobs[i].data() + 0x10, 0xA0, mac_key);
        if (std::memcmp(cmac.data(), encrypted_keyblobs[i].data(), cmac.size()) != 0)
            continue;

        // Decrypt keyblob
        bool has_keyblob = keyblobs[i] != std::array<u8, 0x90>{};

        AESCipher<Key128> cipher(key, Mode::CTR);
        cipher.SetIV(std::vector<u8>(encrypted_keyblobs[i].data() + 0x10,
                                     encrypted_keyblobs[i].data() + 0x20));
        cipher.Transcode(encrypted_keyblobs[i].data() + 0x20, keyblobs[i].size(),
                         keyblobs[i].data(), Op::Decrypt);

        if (!has_keyblob) {
            WriteKeyToFile<0x90>(KeyCategory::Console, fmt::format("keyblob_{:02X}", i),
                                 keyblobs[i]);
        }

        Key128 package1{};
        std::memcpy(package1.data(), keyblobs[i].data() + 0x80, sizeof(Key128));
        SetKey(S128KeyType::Package1, package1, i);

        // Derive master key
        if (HasKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::Master))) {
            Key128 master_root{};
            std::memcpy(master_root.data(), keyblobs[i].data(), sizeof(Key128));

            AESCipher<Key128> master_cipher(master_root, Mode::ECB);

            Key128 master{};
            master_cipher.Transcode(master_source.data(), master_source.size(), master.data(),
                                    Op::Decrypt);
            SetKey(S128KeyType::Master, master, i);
        }
    }

    revisions.set();
    for (size_t i = 0; i < 32; ++i) {
        if (!HasKey(S128KeyType::Master, i))
            revisions.reset(i);
    }

    if (!revisions.any())
        return;

    for (size_t i = 0; i < 32; ++i) {
        if (!revisions[i])
            continue;

        // Derive general purpose keys
        if (HasKey(S128KeyType::Master, i)) {
            for (auto kak_type :
                 {KeyAreaKeyType::Application, KeyAreaKeyType::Ocean, KeyAreaKeyType::System}) {
                if (HasKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::KeyAreaKey),
                           static_cast<u64>(kak_type))) {
                    const auto source =
                        GetKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::KeyAreaKey),
                               static_cast<u64>(kak_type));
                    const auto kek =
                        GenerateKeyEncryptionKey(source, GetKey(S128KeyType::Master, i),
                                                 kek_generation_source, key_generation_source);
                    SetKey(S128KeyType::KeyArea, kek, i, static_cast<u64>(kak_type));
                }
            }

            AESCipher<Key128> master_cipher(GetKey(S128KeyType::Master, i), Mode::ECB);
            for (auto key_type : {SourceKeyType::Titlekek, SourceKeyType::Package2}) {
                if (HasKey(S128KeyType::Source, static_cast<u64>(key_type))) {
                    Key128 key{};
                    master_cipher.Transcode(
                        GetKey(S128KeyType::Source, static_cast<u64>(key_type)).data(), key.size(),
                        key.data(), Op::Decrypt);
                    SetKey(key_type == SourceKeyType::Titlekek ? S128KeyType::Titlekek
                                                               : S128KeyType::Package2,
                           key, i);
                }
            }
        }
    }

    if (HasKey(S128KeyType::Master, 0) &&
        HasKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::AESKeyGeneration)) &&
        HasKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::AESKekGeneration)) &&
        HasKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::HeaderKek)) &&
        HasKey(S256KeyType::HeaderSource)) {
        const auto header_kek = GenerateKeyEncryptionKey(
            GetKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::HeaderKek)),
            GetKey(S128KeyType::Master, 0),
            GetKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::AESKekGeneration)),
            GetKey(S128KeyType::Source, static_cast<u64>(SourceKeyType::AESKeyGeneration)));
        SetKey(S128KeyType::HeaderKek, header_kek);

        AESCipher<Key128> header_cipher(header_kek, Mode::ECB);
        Key256 out = GetKey(S256KeyType::HeaderSource);
        header_cipher.Transcode(out.data(), out.size(), out.data(), Op::Decrypt);
        SetKey(S256KeyType::Header, out);
    }
}
void KeyManager::SetKeyWrapped(S128KeyType id, Key128 key, u64 field1, u64 field2) {
    if (key == Key128{})
        return;
    SetKey(id, key, field1, field2);
}

void KeyManager::SetKeyWrapped(S256KeyType id, Key256 key, u64 field1, u64 field2) {
    if (key == Key256{})
        return;
    SetKey(id, key, field1, field2);
}

const boost::container::flat_map<std::string, KeyIndex<S128KeyType>> KeyManager::s128_file_id = {
    {"eticket_rsa_kek", {S128KeyType::ETicketRSAKek, 0, 0}},
    {"eticket_rsa_kek_source",
     {S128KeyType::Source, static_cast<u64>(SourceKeyType::ETicketKek), 0}},
    {"eticket_rsa_kekek_source",
     {S128KeyType::Source, static_cast<u64>(SourceKeyType::ETicketKekek), 0}},
    {"rsa_kek_mask_0", {S128KeyType::RSAKek, static_cast<u64>(RSAKekType::Mask0), 0}},
    {"rsa_kek_seed_3", {S128KeyType::RSAKek, static_cast<u64>(RSAKekType::Seed3), 0}},
    {"rsa_oaep_kek_generation_source",
     {S128KeyType::Source, static_cast<u64>(SourceKeyType::RSAOaepKekGeneration), 0}},
    {"sd_card_kek_source", {S128KeyType::Source, static_cast<u64>(SourceKeyType::SDKek), 0}},
    {"aes_kek_generation_source",
     {S128KeyType::Source, static_cast<u64>(SourceKeyType::AESKekGeneration), 0}},
    {"aes_key_generation_source",
     {S128KeyType::Source, static_cast<u64>(SourceKeyType::AESKeyGeneration), 0}},
    {"package2_key_source", {S128KeyType::Source, static_cast<u64>(SourceKeyType::Package2), 0}},
    {"master_key_source", {S128KeyType::Source, static_cast<u64>(SourceKeyType::Master), 0}},
    {"header_kek_source", {S128KeyType::Source, static_cast<u64>(SourceKeyType::HeaderKek), 0}},
    {"key_area_key_application_source",
     {S128KeyType::Source, static_cast<u64>(SourceKeyType::KeyAreaKey),
      static_cast<u64>(KeyAreaKeyType::Application)}},
    {"key_area_key_ocean_source",
     {S128KeyType::Source, static_cast<u64>(SourceKeyType::KeyAreaKey),
      static_cast<u64>(KeyAreaKeyType::Ocean)}},
    {"key_area_key_system_source",
     {S128KeyType::Source, static_cast<u64>(SourceKeyType::KeyAreaKey),
      static_cast<u64>(KeyAreaKeyType::System)}},
    {"titlekek_source", {S128KeyType::Source, static_cast<u64>(SourceKeyType::Titlekek), 0}},
    {"keyblob_mac_key_source", {S128KeyType::Source, static_cast<u64>(SourceKeyType::KeyblobMAC)}},
    {"tsec_key", {S128KeyType::TSEC, 0, 0}},
    {"secure_boot_key", {S128KeyType::SecureBoot, 0, 0}},
    {"sd_seed", {S128KeyType::SDSeed, 0, 0}},
    {"bis_key_0_crypt", {S128KeyType::BIS, 0, static_cast<u64>(BISKeyType::Crypto)}},
    {"bis_key_0_tweak", {S128KeyType::BIS, 0, static_cast<u64>(BISKeyType::Tweak)}},
    {"bis_key_1_crypt", {S128KeyType::BIS, 1, static_cast<u64>(BISKeyType::Crypto)}},
    {"bis_key_1_tweak", {S128KeyType::BIS, 1, static_cast<u64>(BISKeyType::Tweak)}},
    {"bis_key_2_crypt", {S128KeyType::BIS, 2, static_cast<u64>(BISKeyType::Crypto)}},
    {"bis_key_2_tweak", {S128KeyType::BIS, 2, static_cast<u64>(BISKeyType::Tweak)}},
    {"bis_key_3_crypt", {S128KeyType::BIS, 3, static_cast<u64>(BISKeyType::Crypto)}},
    {"bis_key_3_tweak", {S128KeyType::BIS, 3, static_cast<u64>(BISKeyType::Tweak)}},
    {"header_kek", {S128KeyType::HeaderKek, 0, 0}},
    {"sd_card_kek", {S128KeyType::SDKek, 0, 0}},
};

const boost::container::flat_map<std::string, KeyIndex<S256KeyType>> KeyManager::s256_file_id = {
    {"header_key", {S256KeyType::Header, 0, 0}},
    {"sd_card_save_key_source", {S256KeyType::SDKeySource, static_cast<u64>(SDKeyType::Save), 0}},
    {"sd_card_nca_key_source", {S256KeyType::SDKeySource, static_cast<u64>(SDKeyType::NCA), 0}},
    {"header_key_source", {S256KeyType::HeaderSource, 0, 0}},
    {"sd_card_save_key", {S256KeyType::SDKey, static_cast<u64>(SDKeyType::Save), 0}},
    {"sd_card_nca_key", {S256KeyType::SDKey, static_cast<u64>(SDKeyType::NCA), 0}},
};
} // namespace Core::Crypto
