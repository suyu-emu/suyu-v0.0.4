// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>
#include "common/assert.h"
#include "core/file_sys/vfs.h"
#include "common/hex_util.h"
#include "key_manager.h"
#include "mbedtls/cipher.h"

namespace Crypto {
typedef std::array<u8, 0x20> SHA256Hash;

inline SHA256Hash operator"" _HASH(const char* data, size_t len) {
    if (len != 0x40)
        return {};

    return Common::HexStringToArray<0x20>(std::string_view(data, len));
}

} // namespace Crypto
