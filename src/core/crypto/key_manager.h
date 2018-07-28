// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include <array>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"

namespace Crypto {

typedef std::array<u8, 0x10> Key128;
typedef std::array<u8, 0x20> Key256;
typedef std::array<u8, 0x20> SHA256Hash;

static_assert(sizeof(Key128) == 16, "Key128 must be 128 bytes big.");
static_assert(sizeof(Key256) == 32, "Key128 must be 128 bytes big.");

enum class S256KeyType : u64 {
    HEADER,  //
    SD_SAVE, //
    SD_NCA,  //
};

enum class S128KeyType : u64 {
    MASTER,          // f1=crypto revision
    PACKAGE1,        // f1=crypto revision
    PACKAGE2,        // f1=crypto revision
    TITLEKEK,        // f1=crypto revision
    ETICKET_RSA_KEK, //
    KEY_AREA,        // f1=crypto revision f2=type {app, ocean, system}
    SD_SEED,         //
    TITLEKEY,        // f1=rights id LSB f2=rights id MSB
};

enum class KeyAreaKeyType : u8 {
    Application,
    Ocean,
    System,
};

template <typename KeyType>
struct KeyIndex {
    KeyType type;
    u64 field1;
    u64 field2;

    std::string DebugInfo() {
        u8 key_size = 16;
        if (std::is_same_v<KeyType, S256KeyType>)
            key_size = 32;
        return fmt::format("key_size={:02X}, key={:02X}, field1={:016X}, field2={:016X}", key_size,
                           static_cast<u8>(type), field1, field2);
    }
};

// The following two (== and hash) are so KeyIndex can be a key in unordered_map

template <typename KeyType>
bool operator==(const KeyIndex<KeyType>& lhs, const KeyIndex<KeyType>& rhs) {
    return lhs.type == rhs.type && lhs.field1 == rhs.field1 && lhs.field2 == rhs.field2;
}

} // namespace Crypto

namespace std {
template <typename KeyType>
struct hash<Crypto::KeyIndex<KeyType>> {
    size_t operator()(const Crypto::KeyIndex<KeyType>& k) const {
        using std::hash;

        return ((hash<u64>()(static_cast<u64>(k.type)) ^ (hash<u64>()(k.field1) << 1)) >> 1) ^
               (hash<u64>()(k.field2) << 1);
    }
};
} // namespace std

namespace Crypto {

std::array<u8, 0x10> operator"" _array16(const char* str, size_t len);
std::array<u8, 0x20> operator"" _array32(const char* str, size_t len);

struct KeyManager {
    void SetValidationMode(bool dev);
    void LoadFromFile(std::string_view filename, bool is_title_keys);

    bool HasKey(S128KeyType id, u64 field1 = 0, u64 field2 = 0);
    bool HasKey(S256KeyType id, u64 field1 = 0, u64 field2 = 0);

    Key128 GetKey(S128KeyType id, u64 field1 = 0, u64 field2 = 0);
    Key256 GetKey(S256KeyType id, u64 field1 = 0, u64 field2 = 0);

    void SetKey(S128KeyType id, Key128 key, u64 field1 = 0, u64 field2 = 0);
    void SetKey(S256KeyType id, Key256 key, u64 field1 = 0, u64 field2 = 0);

    bool ValidateKey(S128KeyType key, u64 field1 = 0, u64 field2 = 0);
    bool ValidateKey(S256KeyType key, u64 field1 = 0, u64 field2 = 0);

private:
    std::unordered_map<KeyIndex<S128KeyType>, Key128> s128_keys;
    std::unordered_map<KeyIndex<S256KeyType>, Key256> s256_keys;

    bool dev_mode = false;

    static std::unordered_map<KeyIndex<S128KeyType>, SHA256Hash> s128_hash_prod;
    static std::unordered_map<KeyIndex<S256KeyType>, SHA256Hash> s256_hash_prod;
    static std::unordered_map<KeyIndex<S128KeyType>, SHA256Hash> s128_hash_dev;
    static std::unordered_map<KeyIndex<S256KeyType>, SHA256Hash> s256_hash_dev;
    static std::unordered_map<std::string, KeyIndex<S128KeyType>> s128_file_id;
    static std::unordered_map<std::string, KeyIndex<S256KeyType>> s256_file_id;
};

extern KeyManager keys;

} // namespace Crypto
