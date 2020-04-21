// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/result.h"

namespace AudioCore {
namespace Audren {
constexpr ResultCode ERR_INVALID_PARAMETERS{ErrorModule::Audio, 41};
}

constexpr u32_le CURRENT_PROCESS_REVISION = Common::MakeMagic('R', 'E', 'V', '8');

static constexpr u32 VersionFromRevision(u32_le rev) {
    // "REV7" -> 7
    return ((rev >> 24) & 0xff) - 0x30;
}

static constexpr bool IsRevisionSupported(u32 required, u32_le user_revision) {
    const auto base = VersionFromRevision(user_revision);
    return required <= base;
}

static constexpr bool IsValidRevision(u32_le revision) {
    const auto base = VersionFromRevision(revision);
    constexpr auto max_rev = VersionFromRevision(CURRENT_PROCESS_REVISION);
    return base <= max_rev;
}

static constexpr bool CanConsumeBuffer(std::size_t size, std::size_t offset, std::size_t required) {
    if (offset > size) {
        return false;
    }
    if (size < required) {
        return false;
    }
    if ((size - offset) < required) {
        return false;
    }
    return true;
}

} // namespace AudioCore
