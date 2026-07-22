// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <span>
#include <type_traits>
#include "common/common_types.h"

namespace Core::Crypto {

struct CipherContext;

enum class Mode {
    CTR = 11,
    ECB = 2,
    XTS = 74,
};

enum class Op {
    Encrypt,
    Decrypt,
};

template <typename Key>
class AESCipher {
public:
    static_assert(sizeof(Key) == 0x10 || sizeof(Key) == 0x20);
    AESCipher(Key key, Mode mode);
    ~AESCipher();

    void SetIV(std::span<const u8> data);

    template <typename Source, typename Dest>
        requires std::is_trivially_copyable_v<Source> && std::is_trivially_copyable_v<Dest>
    void Transcode(const Source* src, std::size_t size, Dest* dest, Op op) const {
        Transcode(reinterpret_cast<const u8*>(src), size, reinterpret_cast<u8*>(dest), op);
    }

    void Transcode(const u8* src, std::size_t size, u8* dest, Op op) const;

    template <typename Source, typename Dest>
        requires std::is_trivially_copyable_v<Source> && std::is_trivially_copyable_v<Dest>
    void XTSTranscode(const Source* src, std::size_t size, Dest* dest, std::size_t sector_id, std::size_t sector_size, Op op) {
        XTSTranscode(reinterpret_cast<const u8*>(src), size, reinterpret_cast<u8*>(dest), sector_id, sector_size, op);
    }

    void XTSTranscode(const u8* src, std::size_t size, u8* dest, std::size_t sector_id, std::size_t sector_size, Op op);

private:
    std::unique_ptr<CipherContext> ctx;
};

} // namespace Core::Crypto
