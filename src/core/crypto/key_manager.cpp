// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fstream>
#include <locale>
#include <sstream>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/crypto/key_manager.h"
#include "mbedtls/sha256.h"

namespace Crypto {
KeyManager keys = {};

std::unordered_map<KeyIndex<S128KeyType>, SHA256Hash> KeyManager::s128_hash_prod = {
    {{S128KeyType::MASTER, 0, 0},
     "0EE359BE3C864BB0782E1D70A718A0342C551EED28C369754F9C4F691BECF7CA"_array32},
    {{S128KeyType::MASTER, 1, 0},
     "4FE707B7E4ABDAF727C894AAF13B1351BFE2AC90D875F73B2E20FA94B9CC661E"_array32},
    {{S128KeyType::MASTER, 2, 0},
     "79277C0237A2252EC3DFAC1F7C359C2B3D121E9DB15BB9AB4C2B4408D2F3AE09"_array32},
    {{S128KeyType::MASTER, 3, 0},
     "4F36C565D13325F65EE134073C6A578FFCB0008E02D69400836844EAB7432754"_array32},
    {{S128KeyType::MASTER, 4, 0},
     "75ff1d95d26113550ee6fcc20acb58e97edeb3a2ff52543ed5aec63bdcc3da50"_array32},
    {{S128KeyType::PACKAGE1, 0, 0},
     "4543CD1B7CAD7EE0466A3DE2086A0EF923805DCEA6C741541CDDB14F54F97B40"_array32},
    {{S128KeyType::PACKAGE1, 1, 0},
     "4A11DA019D26470C9B805F1721364830DC0096DD66EAC453B0D14455E5AF5CF8"_array32},
    {{S128KeyType::PACKAGE1, 2, 0},
     "CCA867360B3318246FBF0B8A86473176ED486DFE229772B941A02E84D50A3155"_array32},
    {{S128KeyType::PACKAGE1, 3, 0},
     "E65C383CDF526DFFAA77682868EBFA9535EE60D8075C961BBC1EDE5FBF7E3C5F"_array32},
    {{S128KeyType::PACKAGE1, 4, 0},
     "28ae73d6ae8f7206fca549e27097714e599df1208e57099416ff429b71370162"_array32},
    {{S128KeyType::PACKAGE2, 0, 0},
     "94D6F38B9D0456644E21DFF4707D092B70179B82D1AA2F5B6A76B8F9ED948264"_array32},
    {{S128KeyType::PACKAGE2, 1, 0},
     "7794F24FA879D378FEFDC8776B949B88AD89386410BE9025D463C619F1530509"_array32},
    {{S128KeyType::PACKAGE2, 2, 0},
     "5304BDDE6AC8E462961B5DB6E328B1816D245D36D6574BB78938B74D4418AF35"_array32},
    {{S128KeyType::PACKAGE2, 3, 0},
     "BE1E52C4345A979DDD4924375B91C902052C2E1CF8FBF2FAA42E8F26D5125B60"_array32},
    {{S128KeyType::PACKAGE2, 4, 0},
     "631b45d349ab8f76a050fe59512966fb8dbaf0755ef5b6903048bf036cfa611e"_array32},
    {{S128KeyType::TITLEKEK, 0, 0},
     "C2FA30CAC6AE1680466CB54750C24550E8652B3B6F38C30B49DADF067B5935E9"_array32},
    {{S128KeyType::TITLEKEK, 1, 0},
     "0D6B8F3746AD910D36438A859C11E8BE4310112425D63751D09B5043B87DE598"_array32},
    {{S128KeyType::TITLEKEK, 2, 0},
     "D09E18D3DB6BC7393536896F728528736FBEFCDD15C09D9D612FDE5C7BDCD821"_array32},
    {{S128KeyType::TITLEKEK, 3, 0},
     "47C6F9F7E99BB1F56DCDC93CDBD340EA82DCCD74DD8F3535ADA20ECF79D438ED"_array32},
    {{S128KeyType::TITLEKEK, 4, 0},
     "128610de8424cb29e08f9ee9a81c9e6ffd3c6662854aad0c8f937e0bcedc4d88"_array32},
    {{S128KeyType::ETICKET_RSA_KEK, 0, 0},
     "46cccf288286e31c931379de9efa288c95c9a15e40b00a4c563a8be244ece515"_array32},
    {{S128KeyType::KEY_AREA, 0, static_cast<u64>(KeyAreaKeyType::Application)},
     "592957F44FE5DB5EC6B095F568910E31A226D3B7FE42D64CFB9CE4051E90AEB6"_array32},
    {{S128KeyType::KEY_AREA, 1, static_cast<u64>(KeyAreaKeyType::Application)},
     "C2252A0FBF9D339ABC3D681351D00452F926E7CA0C6CA85F659078DE3FA647F3"_array32},
    {{S128KeyType::KEY_AREA, 2, static_cast<u64>(KeyAreaKeyType::Application)},
     "7C7722824B2F7C4938C40F3EA93E16CB69D3285EB133490EF8ECCD2C4B52DF41"_array32},
    {{S128KeyType::KEY_AREA, 3, static_cast<u64>(KeyAreaKeyType::Application)},
     "AFBB8EBFB2094F1CF71E330826AE06D64414FCA128C464618DF30EED92E62BE6"_array32},
    {{S128KeyType::KEY_AREA, 4, static_cast<u64>(KeyAreaKeyType::Application)},
     "5dc10eb81918da3f2fa90f69c8542511963656cfb31fb7c779581df8faf1f2f5"_array32},
    {{S128KeyType::KEY_AREA, 0, static_cast<u64>(KeyAreaKeyType::Ocean)},
     "AA2C65F0E27F730807A13F2ED5B99BE5183165B87C50B6ED48F5CAC2840687EB"_array32},
    {{S128KeyType::KEY_AREA, 1, static_cast<u64>(KeyAreaKeyType::Ocean)},
     "860185F2313A14F7006A029CB21A52750E7718C1E94FFB98C0AE2207D1A60165"_array32},
    {{S128KeyType::KEY_AREA, 2, static_cast<u64>(KeyAreaKeyType::Ocean)},
     "7283FB1EFBD42438DADF363FDB776ED355C98737A2AAE75D0E9283CE1C12A2E4"_array32},
    {{S128KeyType::KEY_AREA, 3, static_cast<u64>(KeyAreaKeyType::Ocean)},
     "9881C2D3AB70B14C8AA12016FC73ADAD93C6AD9FB59A9ECAD312B6F89E2413EC"_array32},
    {{S128KeyType::KEY_AREA, 4, static_cast<u64>(KeyAreaKeyType::Ocean)},
     "eaa6a8d242b89e174928fa9549a0f66ec1562e2576fac896f438a2b3c1fb6005"_array32},
    {{S128KeyType::KEY_AREA, 0, static_cast<u64>(KeyAreaKeyType::System)},
     "194CF6BD14554DA8D457E14CBFE04E55C8FB8CA52E0AFB3D7CB7084AE435B801"_array32},
    {{S128KeyType::KEY_AREA, 1, static_cast<u64>(KeyAreaKeyType::System)},
     "CE1DB7BB6E5962384889DB7A396AFD614F82F69DC38A33D2DEAF47F3E4B964B7"_array32},
    {{S128KeyType::KEY_AREA, 2, static_cast<u64>(KeyAreaKeyType::System)},
     "42238DE5685DEF4FDE7BE42C0097CEB92447006386D6B5D5AAA2C9AFD2E28422"_array32},
    {{S128KeyType::KEY_AREA, 3, static_cast<u64>(KeyAreaKeyType::System)},
     "1F6847F268E9D9C5D1AD4D7E226A63B833BF02071446957A962EF065521879C1"_array32},
    {{S128KeyType::KEY_AREA, 4, static_cast<u64>(KeyAreaKeyType::System)},
     "644007f9913c3602399d4d75cc34faeb7f1faad18b23e34187b16fdc45f4980f"_array32},
};

std::unordered_map<KeyIndex<S256KeyType>, SHA256Hash> KeyManager::s256_hash_prod = {
    {{S256KeyType::HEADER, 0, 0},
     "8E03DE24818D96CE4F2A09B43AF979E679974F7570713A61EED8B314864A11D5"_array32},
    {{S256KeyType::SD_SAVE, 0, 0},
     "13020ee72d0f8b8f9112dc738b829fdb017102499a7c2259b52aeefc0a273f5c"_array32},
    {{S256KeyType::SD_NCA, 0, 0},
     "8a1c05b4f88bae5b04d77f632e6acfc8893c4a05fd701f53585daafc996b532a"_array32},
};

// TODO(DarkLordZach): Find missing hashes for dev keys.

std::unordered_map<KeyIndex<S128KeyType>, SHA256Hash> KeyManager::s128_hash_dev = {
    {{S128KeyType::MASTER, 0, 0},
     "779dd8b533a2fb670f27b308cb8d0151c4a107568b817429172b7f80aa592c25"_array32},
    {{S128KeyType::MASTER, 1, 0},
     "0175c8bc49771576f75527be719098db4ebaf77707206749415663aa3a9ea9cc"_array32},
    {{S128KeyType::MASTER, 2, 0},
     "4f0b4d724e5a8787268157c7ce0767c26d2e2021832aa7020f306d6e260eea42"_array32},
    {{S128KeyType::MASTER, 3, 0},
     "7b5a29586c1f84f66fbfabb94518fc45408bb8e5445253d063dda7cfef2a818c"_array32},
    {{S128KeyType::MASTER, 4, 0},
     "87a61dbb05a8755de7fe069562aab38ebfb266c9eb835f09fa62dacc89c98341"_array32},
    {{S128KeyType::PACKAGE1, 0, 0},
     "166510bc63ae50391ebe4ee4ff90ca31cd0e2dd0ff6be839a2f573ec146cc23a"_array32},
    {{S128KeyType::PACKAGE1, 1, 0},
     "f74cd01b86743139c920ec54a8116c669eea805a0be1583e13fc5bc8de68645b"_array32},
    {{S128KeyType::PACKAGE1, 2, 0},
     "d0cdecd513bb6aa3d9dc6244c977dc8a5a7ea157d0a8747d79e7581146e1f768"_array32},
    {{S128KeyType::PACKAGE1, 3, 0},
     "aa39394d626b3b79f5b7ccc07378b5996b6d09bf0eb6771b0b40c9077fbfde8c"_array32},
    {{S128KeyType::PACKAGE1, 4, 0},
     "8f4754b8988c0e673fc2bbea0534cdd6075c815c9270754ae980aef3e4f0a508"_array32},
    {{S128KeyType::PACKAGE2, 0, 0},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::PACKAGE2, 1, 0},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::PACKAGE2, 2, 0},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::PACKAGE2, 3, 0},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::PACKAGE2, 4, 0},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::TITLEKEK, 0, 0},
     "C2FA30CAC6AE1680466CB54750C24550E8652B3B6F38C30B49DADF067B5935E9"_array32},
    {{S128KeyType::TITLEKEK, 1, 0},
     "0D6B8F3746AD910D36438A859C11E8BE4310112425D63751D09B5043B87DE598"_array32},
    {{S128KeyType::TITLEKEK, 2, 0},
     "D09E18D3DB6BC7393536896F728528736FBEFCDD15C09D9D612FDE5C7BDCD821"_array32},
    {{S128KeyType::TITLEKEK, 3, 0},
     "47C6F9F7E99BB1F56DCDC93CDBD340EA82DCCD74DD8F3535ADA20ECF79D438ED"_array32},
    {{S128KeyType::TITLEKEK, 4, 0},
     "128610de8424cb29e08f9ee9a81c9e6ffd3c6662854aad0c8f937e0bcedc4d88"_array32},
    {{S128KeyType::ETICKET_RSA_KEK, 0, 0},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 0, static_cast<u64>(KeyAreaKeyType::Application)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 1, static_cast<u64>(KeyAreaKeyType::Application)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 2, static_cast<u64>(KeyAreaKeyType::Application)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 3, static_cast<u64>(KeyAreaKeyType::Application)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 4, static_cast<u64>(KeyAreaKeyType::Application)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 0, static_cast<u64>(KeyAreaKeyType::Ocean)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 1, static_cast<u64>(KeyAreaKeyType::Ocean)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 2, static_cast<u64>(KeyAreaKeyType::Ocean)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 3, static_cast<u64>(KeyAreaKeyType::Ocean)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 4, static_cast<u64>(KeyAreaKeyType::Ocean)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 0, static_cast<u64>(KeyAreaKeyType::System)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 1, static_cast<u64>(KeyAreaKeyType::System)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 2, static_cast<u64>(KeyAreaKeyType::System)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 3, static_cast<u64>(KeyAreaKeyType::System)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S128KeyType::KEY_AREA, 4, static_cast<u64>(KeyAreaKeyType::System)},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
};

std::unordered_map<KeyIndex<S256KeyType>, SHA256Hash> KeyManager::s256_hash_dev = {
    {{S256KeyType::HEADER, 0, 0},
     "ecde86a76e37ac4fd7591d3aa55c00cc77d8595fc27968052ec18a177d939060"_array32},
    {{S256KeyType::SD_SAVE, 0, 0},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
    {{S256KeyType::SD_NCA, 0, 0},
     "0000000000000000000000000000000000000000000000000000000000000000"_array32},
};

std::unordered_map<std::string, KeyIndex<S128KeyType>> KeyManager::s128_file_id = {
    {"master_key_00", {S128KeyType::MASTER, 0, 0}},
    {"master_key_01", {S128KeyType::MASTER, 1, 0}},
    {"master_key_02", {S128KeyType::MASTER, 2, 0}},
    {"master_key_03", {S128KeyType::MASTER, 3, 0}},
    {"master_key_04", {S128KeyType::MASTER, 4, 0}},
    {"package1_key_00", {S128KeyType::PACKAGE1, 0, 0}},
    {"package1_key_01", {S128KeyType::PACKAGE1, 1, 0}},
    {"package1_key_02", {S128KeyType::PACKAGE1, 2, 0}},
    {"package1_key_03", {S128KeyType::PACKAGE1, 3, 0}},
    {"package1_key_04", {S128KeyType::PACKAGE1, 4, 0}},
    {"package2_key_00", {S128KeyType::PACKAGE2, 0, 0}},
    {"package2_key_01", {S128KeyType::PACKAGE2, 1, 0}},
    {"package2_key_02", {S128KeyType::PACKAGE2, 2, 0}},
    {"package2_key_03", {S128KeyType::PACKAGE2, 3, 0}},
    {"package2_key_04", {S128KeyType::PACKAGE2, 4, 0}},
    {"titlekek_00", {S128KeyType::TITLEKEK, 0, 0}},
    {"titlekek_01", {S128KeyType::TITLEKEK, 1, 0}},
    {"titlekek_02", {S128KeyType::TITLEKEK, 2, 0}},
    {"titlekek_03", {S128KeyType::TITLEKEK, 3, 0}},
    {"titlekek_04", {S128KeyType::TITLEKEK, 4, 0}},
    {"eticket_rsa_kek", {S128KeyType::ETICKET_RSA_KEK, 0, 0}},
    {"key_area_key_application_00",
     {S128KeyType::KEY_AREA, 0, static_cast<u64>(KeyAreaKeyType::Application)}},
    {"key_area_key_application_01",
     {S128KeyType::KEY_AREA, 1, static_cast<u64>(KeyAreaKeyType::Application)}},
    {"key_area_key_application_02",
     {S128KeyType::KEY_AREA, 2, static_cast<u64>(KeyAreaKeyType::Application)}},
    {"key_area_key_application_03",
     {S128KeyType::KEY_AREA, 3, static_cast<u64>(KeyAreaKeyType::Application)}},
    {"key_area_key_application_04",
     {S128KeyType::KEY_AREA, 4, static_cast<u64>(KeyAreaKeyType::Application)}},
    {"key_area_key_ocean_00", {S128KeyType::KEY_AREA, 0, static_cast<u64>(KeyAreaKeyType::Ocean)}},
    {"key_area_key_ocean_01", {S128KeyType::KEY_AREA, 1, static_cast<u64>(KeyAreaKeyType::Ocean)}},
    {"key_area_key_ocean_02", {S128KeyType::KEY_AREA, 2, static_cast<u64>(KeyAreaKeyType::Ocean)}},
    {"key_area_key_ocean_03", {S128KeyType::KEY_AREA, 3, static_cast<u64>(KeyAreaKeyType::Ocean)}},
    {"key_area_key_ocean_04", {S128KeyType::KEY_AREA, 4, static_cast<u64>(KeyAreaKeyType::Ocean)}},
    {"key_area_key_system_00",
     {S128KeyType::KEY_AREA, 0, static_cast<u64>(KeyAreaKeyType::System)}},
    {"key_area_key_system_01",
     {S128KeyType::KEY_AREA, 1, static_cast<u64>(KeyAreaKeyType::System)}},
    {"key_area_key_system_02",
     {S128KeyType::KEY_AREA, 2, static_cast<u64>(KeyAreaKeyType::System)}},
    {"key_area_key_system_03",
     {S128KeyType::KEY_AREA, 3, static_cast<u64>(KeyAreaKeyType::System)}},
    {"key_area_key_system_04",
     {S128KeyType::KEY_AREA, 4, static_cast<u64>(KeyAreaKeyType::System)}},
};

std::unordered_map<std::string, KeyIndex<S256KeyType>> KeyManager::s256_file_id = {
    {"header_key", {S256KeyType::HEADER, 0, 0}},
    {"sd_card_save_key", {S256KeyType::SD_SAVE, 0, 0}},
    {"sd_card_nca_key", {S256KeyType::SD_NCA, 0, 0}},
};

static u8 ToHexNibble(char c1) {
    if (c1 >= 65 && c1 <= 70)
        return c1 - 55;
    if (c1 >= 97 && c1 <= 102)
        return c1 - 87;
    if (c1 >= 48 && c1 <= 57)
        return c1 - 48;
    throw std::logic_error("Invalid hex digit");
}

template <size_t Size>
static std::array<u8, Size> HexStringToArray(std::string_view str) {
    std::array<u8, Size> out{};
    for (size_t i = 0; i < 2 * Size; i += 2) {
        auto d1 = str[i];
        auto d2 = str[i + 1];
        out[i / 2] = (ToHexNibble(d1) << 4) | ToHexNibble(d2);
    }
    return out;
}

std::array<u8, 16> operator""_array16(const char* str, size_t len) {
    if (len != 32)
        throw std::logic_error("Not of correct size.");
    return HexStringToArray<16>(str);
}

std::array<u8, 32> operator""_array32(const char* str, size_t len) {
    if (len != 64)
        throw std::logic_error("Not of correct size.");
    return HexStringToArray<32>(str);
}

void KeyManager::SetValidationMode(bool dev) {
    dev_mode = dev;
}

void KeyManager::LoadFromFile(std::string_view filename_, bool is_title_keys) {
    const auto filename = std::string(filename_);
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

        if (is_title_keys) {
            auto rights_id_raw = HexStringToArray<16>(out[0]);
            u128 rights_id = *reinterpret_cast<std::array<u64, 2>*>(&rights_id_raw);
            Key128 key = HexStringToArray<16>(out[1]);
            SetKey(S128KeyType::TITLEKEY, key, rights_id[1], rights_id[0]);
        } else {
            std::transform(out[0].begin(), out[0].end(), out[0].begin(), ::tolower);
            if (s128_file_id.find(out[0]) != s128_file_id.end()) {
                const auto index = s128_file_id[out[0]];
                Key128 key = HexStringToArray<16>(out[1]);
                SetKey(index.type, key, index.field1, index.field2);
            } else if (s256_file_id.find(out[0]) != s256_file_id.end()) {
                const auto index = s256_file_id[out[0]];
                Key256 key = HexStringToArray<32>(out[1]);
                SetKey(index.type, key, index.field1, index.field2);
            }
        }
    }
}

bool KeyManager::HasKey(S128KeyType id, u64 field1, u64 field2) {
    return s128_keys.find({id, field1, field2}) != s128_keys.end();
}

bool KeyManager::HasKey(S256KeyType id, u64 field1, u64 field2) {
    return s256_keys.find({id, field1, field2}) != s256_keys.end();
}

Key128 KeyManager::GetKey(S128KeyType id, u64 field1, u64 field2) {
    if (!HasKey(id, field1, field2))
        return {};
    return s128_keys[{id, field1, field2}];
}

Key256 KeyManager::GetKey(S256KeyType id, u64 field1, u64 field2) {
    if (!HasKey(id, field1, field2))
        return {};
    return s256_keys[{id, field1, field2}];
}

void KeyManager::SetKey(S128KeyType id, Key128 key, u64 field1, u64 field2) {
    s128_keys[{id, field1, field2}] = key;
}

void KeyManager::SetKey(S256KeyType id, Key256 key, u64 field1, u64 field2) {
    s256_keys[{id, field1, field2}] = key;
}

bool KeyManager::ValidateKey(S128KeyType key, u64 field1, u64 field2) {
    auto& hash = dev_mode ? s128_hash_dev : s128_hash_prod;

    KeyIndex<S128KeyType> id = {key, field1, field2};
    if (key == S128KeyType::SD_SEED || key == S128KeyType::TITLEKEY ||
        hash.find(id) == hash.end()) {
        LOG_WARNING(Crypto, "Could not validate [{}]", id.DebugInfo());
        return true;
    }

    if (!HasKey(key, field1, field2)) {
        LOG_CRITICAL(Crypto,
                     "System has requested validation of [{}], but user has not added it. Add this "
                     "key to use functionality.",
                     id.DebugInfo());
        return false;
    }

    SHA256Hash key_hash{};
    const auto a_key = GetKey(key, field1, field2);
    mbedtls_sha256(a_key.data(), a_key.size(), key_hash.data(), 0);
    if (key_hash != hash[id]) {
        LOG_CRITICAL(Crypto,
                     "The hash of the provided key for [{}] does not match the one on file. This "
                     "means you probably have an incorrect key. If you believe this to be in "
                     "error, contact the yuzu devs.",
                     id.DebugInfo());
        return false;
    }

    return true;
}

bool KeyManager::ValidateKey(S256KeyType key, u64 field1, u64 field2) {
    auto& hash = dev_mode ? s256_hash_dev : s256_hash_prod;

    KeyIndex<S256KeyType> id = {key, field1, field2};
    if (hash.find(id) == hash.end()) {
        LOG_ERROR(Crypto, "Could not validate [{}]", id.DebugInfo());
        return true;
    }

    if (!HasKey(key, field1, field2)) {
        LOG_ERROR(Crypto,
                  "System has requested validation of [{}], but user has not added it. Add this "
                  "key to use functionality.",
                  id.DebugInfo());
        return false;
    }

    SHA256Hash key_hash{};
    const auto a_key = GetKey(key, field1, field2);
    mbedtls_sha256(a_key.data(), a_key.size(), key_hash.data(), 0);
    if (key_hash != hash[id]) {
        LOG_CRITICAL(Crypto,
                     "The hash of the provided key for [{}] does not match the one on file. This "
                     "means you probably have an incorrect key. If you believe this to be in "
                     "error, contact the yuzu devs.",
                     id.DebugInfo());
        return false;
    }

    return true;
}
} // namespace Crypto
