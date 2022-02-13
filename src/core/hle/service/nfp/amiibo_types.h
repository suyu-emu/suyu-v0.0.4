// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>

namespace Service::NFP {
enum class ServiceType : u32 {
    User,
    Debug,
    System,
};

enum class State : u32 {
    NonInitialized,
    Initialized,
};

enum class DeviceState : u32 {
    Initialized,
    SearchingForTag,
    TagFound,
    TagRemoved,
    TagMounted,
    Unaviable,
    Finalized,
};

enum class ModelType : u32 {
    Amiibo,
};

enum class MountTarget : u32 {
    Rom,
    Ram,
    All,
};

enum class AmiiboType : u8 {
    Figure,
    Card,
    Yarn,
};

enum class AmiiboSeries : u8 {
    SuperSmashBros,
    SuperMario,
    ChibiRobo,
    YoshiWoollyWorld,
    Splatoon,
    AnimalCrossing,
    EightBitMario,
    Skylanders,
    Unknown8,
    TheLegendOfZelda,
    ShovelKnight,
    Unknown11,
    Kiby,
    Pokemon,
    MarioSportsSuperstars,
    MonsterHunter,
    BoxBoy,
    Pikmin,
    FireEmblem,
    Metroid,
    Others,
    MegaMan,
    Diablo,
};

using TagUuid = std::array<u8, 10>;
using HashData = std::array<u8, 0x20>;
using ApplicationArea = std::array<u8, 0xD8>;

struct AmiiboDate {
    union {
        u16_be raw{};

        BitField<0, 5, u16> day;
        BitField<5, 4, u16> month;
        BitField<9, 7, u16> year;
    };
};
static_assert(sizeof(AmiiboDate) == 2, "AmiiboDate is an invalid size");

struct Settings {
    union {
        u8 raw{};

        BitField<4, 1, u8> amiibo_initialized;
        BitField<5, 1, u8> appdata_initialized;
    };
};
static_assert(sizeof(Settings) == 1, "AmiiboDate is an invalid size");

struct AmiiboSettings {
    Settings settings;
    u8 country_code_id;
    u16_be crc_counter; // Incremented each time crc is changed
    AmiiboDate init_date;
    AmiiboDate write_date;
    u32_be crc;
    std::array<u16_be, 0xA> amiibo_name; // UTF-16 text
};
static_assert(sizeof(AmiiboSettings) == 0x20, "AmiiboSettings is an invalid size");

struct AmiiboModelInfo {
    u16 character_id;
    u8 character_variant;
    AmiiboType amiibo_type;
    u16 model_number;
    AmiiboSeries series;
    u8 constant_value;         // Must be 02
    INSERT_PADDING_BYTES(0x4); // Unknown
};
static_assert(sizeof(AmiiboModelInfo) == 0xC, "AmiiboModelInfo is an invalid size");

struct NTAG215Password {
    u32 PWD;  // Password to allow write access
    u16 PACK; // Password acknowledge reply
    u16 RFUI; // Reserved for future use
};
static_assert(sizeof(NTAG215Password) == 0x8, "NTAG215Password is an invalid size");

// Based on citra HLE::Applets::MiiData and PretendoNetwork.
// https://github.com/citra-emu/citra/blob/master/src/core/hle/applets/mii_selector.h#L48
// https://github.com/PretendoNetwork/mii-js/blob/master/mii.js#L299
#pragma pack(1)
struct AmiiboRegisterInfo {
    u32_be mii_id;
    u64_be system_id;
    u32_be specialness_and_creation_date;
    std::array<u8, 0x6> creator_mac;
    u16_be padding;
    union {
        u16 raw;

        BitField<0, 1, u16> gender;
        BitField<1, 4, u16> birth_month;
        BitField<5, 5, u16> birth_day;
        BitField<10, 4, u16> favorite_color;
        BitField<14, 1, u16> favorite;
    } mii_information;
    std::array<char16_t, 0xA> mii_name;
    u8 height;
    u8 build;
    union {
        u8 raw;

        BitField<0, 1, u8> disable_sharing;
        BitField<1, 4, u8> face_shape;
        BitField<5, 3, u8> skin_color;
    } appearance_bits1;
    union {
        u8 raw;

        BitField<0, 4, u8> wrinkles;
        BitField<4, 4, u8> makeup;
    } appearance_bits2;
    u8 hair_style;
    union {
        u8 raw;

        BitField<0, 3, u8> hair_color;
        BitField<3, 1, u8> flip_hair;
    } appearance_bits3;
    union {
        u32 raw;

        BitField<0, 6, u32> eye_type;
        BitField<6, 3, u32> eye_color;
        BitField<9, 4, u32> eye_scale;
        BitField<13, 3, u32> eye_vertical_stretch;
        BitField<16, 5, u32> eye_rotation;
        BitField<21, 4, u32> eye_spacing;
        BitField<25, 5, u32> eye_y_position;
    } appearance_bits4;
    union {
        u32 raw;

        BitField<0, 5, u32> eyebrow_style;
        BitField<5, 3, u32> eyebrow_color;
        BitField<8, 4, u32> eyebrow_scale;
        BitField<12, 3, u32> eyebrow_yscale;
        BitField<16, 4, u32> eyebrow_rotation;
        BitField<21, 4, u32> eyebrow_spacing;
        BitField<25, 5, u32> eyebrow_y_position;
    } appearance_bits5;
    union {
        u16 raw;

        BitField<0, 5, u16> nose_type;
        BitField<5, 4, u16> nose_scale;
        BitField<9, 5, u16> nose_y_position;
    } appearance_bits6;
    union {
        u16 raw;

        BitField<0, 6, u16> mouth_type;
        BitField<6, 3, u16> mouth_color;
        BitField<9, 4, u16> mouth_scale;
        BitField<13, 3, u16> mouth_horizontal_stretch;
    } appearance_bits7;
    union {
        u8 raw;

        BitField<0, 5, u8> mouth_y_position;
        BitField<5, 3, u8> mustache_type;
    } appearance_bits8;
    u8 allow_copying;
    union {
        u16 raw;

        BitField<0, 3, u16> bear_type;
        BitField<3, 3, u16> facial_hair_color;
        BitField<6, 4, u16> mustache_scale;
        BitField<10, 5, u16> mustache_y_position;
    } appearance_bits9;
    union {
        u16 raw;

        BitField<0, 4, u16> glasses_type;
        BitField<4, 3, u16> glasses_color;
        BitField<7, 4, u16> glasses_scale;
        BitField<11, 5, u16> glasses_y_position;
    } appearance_bits10;
    union {
        u16 raw;

        BitField<0, 1, u16> mole_enabled;
        BitField<1, 4, u16> mole_scale;
        BitField<5, 5, u16> mole_x_position;
        BitField<10, 5, u16> mole_y_position;
    } appearance_bits11;

    std::array<u16_le, 0xA> author_name;
    INSERT_PADDING_BYTES(0x4);
};
static_assert(sizeof(AmiiboRegisterInfo) == 0x60, "AmiiboRegisterInfo is an invalid size");

struct EncryptedAmiiboFile {
    u8 constant_value;               // Must be A5
    u16 write_counter;               // Number of times the amiibo has been written?
    INSERT_PADDING_BYTES(0x1);       // Unknown 1
    AmiiboSettings settings;         // Encrypted amiibo settings
    HashData locked_hash;            // Hash
    AmiiboModelInfo model_info;      // Encrypted amiibo model info
    HashData keygen_salt;            // Salt
    HashData unfixed_hash;           // Hash
    AmiiboRegisterInfo owner_mii;    // Encrypted Mii data
    u64_be title_id;                 // Encrypted Game id
    u16_be applicaton_write_counter; // Encrypted Counter
    u32_be application_area_id;      // Encrypted Game id
    std::array<u8, 0x2> unknown;
    HashData hash;                    // Probably a SHA256-HMAC hash?
    ApplicationArea application_area; // Encrypted Game data
};
static_assert(sizeof(EncryptedAmiiboFile) == 0x1F8, "AmiiboFile is an invalid size");

struct NTAG215File {
    std::array<u8, 0x2> uuid2;
    u16 static_lock;           // Set defined pages as read only
    u32 compability_container; // Defines available memory
    HashData unfixed_hash;     // Hash
    u8 constant_value;         // Must be A5
    u16 write_counter;         // Number of times the amiibo has been written?
    INSERT_PADDING_BYTES(0x1); // Unknown 1
    AmiiboSettings settings;
    AmiiboRegisterInfo owner_mii; // Encrypted Mii data
    u64_be title_id;
    u16_be applicaton_write_counter; // Encrypted Counter
    u32_be application_area_id;
    std::array<u8, 0x2> unknown;
    HashData hash;                    // Probably a SHA256-HMAC hash?
    ApplicationArea application_area; // Encrypted Game data
    HashData locked_hash;             // Hash
    std::array<u8, 0x8> uuid;
    AmiiboModelInfo model_info;
    HashData keygen_salt;     // Salt
    u32 dynamic_lock;         // Dynamic lock
    u32 CFG0;                 // Defines memory protected by password
    u32 CFG1;                 // Defines number of verification attempts
    NTAG215Password password; // Password data
};
static_assert(sizeof(NTAG215File) == 0x21C, "NTAG215File is an invalid size");
static_assert(std::is_trivially_copyable_v<NTAG215File>, "NTAG215File must be trivially copyable.");
#pragma pack()

struct EncryptedNTAG215File {
    TagUuid uuid;                    // Unique serial number
    u16 static_lock;                 // Set defined pages as read only
    u32 compability_container;       // Defines available memory
    EncryptedAmiiboFile user_memory; // Writable data
    u32 dynamic_lock;                // Dynamic lock
    u32 CFG0;                        // Defines memory protected by password
    u32 CFG1;                        // Defines number of verification attempts
    NTAG215Password password;        // Password data
};
static_assert(sizeof(EncryptedNTAG215File) == 0x21C, "EncryptedNTAG215File is an invalid size");
static_assert(std::is_trivially_copyable_v<EncryptedNTAG215File>,
              "EncryptedNTAG215File must be trivially copyable.");

} // namespace Service::NFP
