// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This file references various implementation details from Atmosphere, an open-source firmware for
// the Nintendo Switch. Copyright 2018-2020 Atmosphere-NX.

#pragma once

#include "common/common_types.h"

namespace Kernel::Memory {

struct AddressSpaceInfo final {
    enum class Type : u32 {
        Is32Bit = 0,
        Small64Bit = 1,
        Large64Bit = 2,
        Heap = 3,
        Stack = 4,
        Alias = 5,
        Count,
    };

    static u64 GetAddressSpaceStart(std::size_t width, Type type);
    static std::size_t GetAddressSpaceSize(std::size_t width, Type type);

    const std::size_t bit_width{};
    const std::size_t address{};
    const std::size_t size{};
    const Type type{};
};

} // namespace Kernel::Memory
