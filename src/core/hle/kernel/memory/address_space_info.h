// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This file references various implementation details from Atmosphere, an open-source firmware for
// the Nintendo Switch. Copyright 2018-2020 Atmosphere-NX.

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Kernel::Memory {

class AddressSpaceInfo final : NonCopyable {
public:
    enum class Type : u32 {
        Is32Bit = 0,
        Small64Bit = 1,
        Large64Bit = 2,
        Heap = 3,
        Stack = 4,
        Alias = 5,
        Count,
    };

private:
    std::size_t bit_width{};
    std::size_t addr{};
    std::size_t size{};
    Type type{};

public:
    static u64 GetAddressSpaceStart(std::size_t width, Type type);
    static std::size_t GetAddressSpaceSize(std::size_t width, Type type);

    constexpr AddressSpaceInfo(std::size_t bit_width, std::size_t addr, std::size_t size, Type type)
        : bit_width{bit_width}, addr{addr}, size{size}, type{type} {}

    constexpr std::size_t GetWidth() const {
        return bit_width;
    }
    constexpr std::size_t GetAddress() const {
        return addr;
    }
    constexpr std::size_t GetSize() const {
        return size;
    }
    constexpr Type GetType() const {
        return type;
    }
};

} // namespace Kernel::Memory
