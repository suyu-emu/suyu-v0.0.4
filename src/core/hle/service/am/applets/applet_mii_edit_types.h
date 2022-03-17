// Copyright 2022 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/hle/service/mii/types.h"

namespace Service::AM::Applets {

// This is nn::mii::AppletMode
enum class MiiAppletMode : u32 {
    ShowMiiEdit = 0,
    AppendMii = 1,
    AppendMiiImage = 2,
    UpdateMiiImage = 3,
    CreateMii = 4,
    EditMii = 5,
};

struct MiiCharInfo {
    Service::Mii::MiiInfo mii_data{};
    INSERT_PADDING_BYTES(0x28);
};
static_assert(sizeof(MiiCharInfo) == 0x80, "MiiCharInfo has incorrect size.");

// This is nn::mii::AppletInput
struct MiiAppletInput {
    s32 version{};
    MiiAppletMode applet_mode{};
    u32 special_mii_key_code{};
    union {
        std::array<Common::UUID, 8> valid_uuid;
        MiiCharInfo mii_char_info;
    };
    Common::UUID used_uuid;
    INSERT_PADDING_BYTES(0x64);
};
static_assert(sizeof(MiiAppletInput) == 0x100, "MiiAppletInput has incorrect size.");

// This is nn::mii::AppletOutput
struct MiiAppletOutput {
    u32 result{};
    s32 index{};
    INSERT_PADDING_BYTES(0x18);
};
static_assert(sizeof(MiiAppletOutput) == 0x20, "MiiAppletOutput has incorrect size.");

// This is nn::mii::AppletOutputForCharInfoEditing
struct AppletOutputForCharInfoEditing {
    u32 result{};
    Service::Mii::MiiInfo mii_data{};
    INSERT_PADDING_BYTES(0x24);
};
static_assert(sizeof(AppletOutputForCharInfoEditing) == 0x80,
              "AppletOutputForCharInfoEditing has incorrect size.");

} // namespace Service::AM::Applets
