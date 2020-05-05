// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This file references various implementation details from Atmosphere, an open-source firmware for
// the Nintendo Switch. Copyright 2018-2020 Atmosphere-NX.

#include <array>

#include "common/assert.h"
#include "core/hle/kernel/memory/address_space_info.h"

namespace Kernel::Memory {

namespace {

enum : u64 {
    Size_1_MB = 0x100000,
    Size_2_MB = 2 * Size_1_MB,
    Size_128_MB = 128 * Size_1_MB,
    Size_1_GB = 0x40000000,
    Size_2_GB = 2 * Size_1_GB,
    Size_4_GB = 4 * Size_1_GB,
    Size_6_GB = 6 * Size_1_GB,
    Size_64_GB = 64 * Size_1_GB,
    Size_512_GB = 512 * Size_1_GB,
    Invalid = std::numeric_limits<u64>::max(),
};

// clang-format off
constexpr std::array<AddressSpaceInfo, 13> AddressSpaceInfos{{
   { 32 /*bit_width*/, Size_2_MB   /*addr*/, Size_1_GB   - Size_2_MB   /*size*/, AddressSpaceInfo::Type::Is32Bit,    },
   { 32 /*bit_width*/, Size_1_GB   /*addr*/, Size_4_GB   - Size_1_GB   /*size*/, AddressSpaceInfo::Type::Small64Bit, },
   { 32 /*bit_width*/, Invalid     /*addr*/, Size_1_GB                 /*size*/, AddressSpaceInfo::Type::Heap,       },
   { 32 /*bit_width*/, Invalid     /*addr*/, Size_1_GB                 /*size*/, AddressSpaceInfo::Type::Alias,      },
   { 36 /*bit_width*/, Size_128_MB /*addr*/, Size_2_GB   - Size_128_MB /*size*/, AddressSpaceInfo::Type::Is32Bit,    },
   { 36 /*bit_width*/, Size_2_GB   /*addr*/, Size_64_GB  - Size_2_GB   /*size*/, AddressSpaceInfo::Type::Small64Bit, },
   { 36 /*bit_width*/, Invalid     /*addr*/, Size_6_GB                 /*size*/, AddressSpaceInfo::Type::Heap,       },
   { 36 /*bit_width*/, Invalid     /*addr*/, Size_6_GB                 /*size*/, AddressSpaceInfo::Type::Alias,      },
   { 39 /*bit_width*/, Size_128_MB /*addr*/, Size_512_GB - Size_128_MB /*size*/, AddressSpaceInfo::Type::Large64Bit, },
   { 39 /*bit_width*/, Invalid     /*addr*/, Size_64_GB                /*size*/, AddressSpaceInfo::Type::Is32Bit     },
   { 39 /*bit_width*/, Invalid     /*addr*/, Size_6_GB                 /*size*/, AddressSpaceInfo::Type::Heap,       },
   { 39 /*bit_width*/, Invalid     /*addr*/, Size_64_GB                /*size*/, AddressSpaceInfo::Type::Alias,      },
   { 39 /*bit_width*/, Invalid     /*addr*/, Size_2_GB                 /*size*/, AddressSpaceInfo::Type::Stack,      },
}};
// clang-format on

constexpr bool IsAllowedIndexForAddress(std::size_t index) {
    return index < std::size(AddressSpaceInfos) && AddressSpaceInfos[index].GetAddress() != Invalid;
}

constexpr std::array<std::size_t, static_cast<std::size_t>(AddressSpaceInfo::Type::Count)>
    AddressSpaceIndices32Bit{
        0, 1, 0, 2, 0, 3,
    };

constexpr std::array<std::size_t, static_cast<std::size_t>(AddressSpaceInfo::Type::Count)>
    AddressSpaceIndices36Bit{
        4, 5, 4, 6, 4, 7,
    };

constexpr std::array<std::size_t, static_cast<std::size_t>(AddressSpaceInfo::Type::Count)>
    AddressSpaceIndices39Bit{
        9, 8, 8, 10, 12, 11,
    };

constexpr bool IsAllowed32BitType(AddressSpaceInfo::Type type) {
    return type < AddressSpaceInfo::Type::Count && type != AddressSpaceInfo::Type::Large64Bit &&
           type != AddressSpaceInfo::Type::Stack;
}

constexpr bool IsAllowed36BitType(AddressSpaceInfo::Type type) {
    return type < AddressSpaceInfo::Type::Count && type != AddressSpaceInfo::Type::Large64Bit &&
           type != AddressSpaceInfo::Type::Stack;
}

constexpr bool IsAllowed39BitType(AddressSpaceInfo::Type type) {
    return type < AddressSpaceInfo::Type::Count && type != AddressSpaceInfo::Type::Small64Bit;
}

} // namespace

u64 AddressSpaceInfo::GetAddressSpaceStart(std::size_t width, AddressSpaceInfo::Type type) {
    const std::size_t index{static_cast<std::size_t>(type)};
    switch (width) {
    case 32:
        ASSERT(IsAllowed32BitType(type));
        ASSERT(IsAllowedIndexForAddress(AddressSpaceIndices32Bit[index]));
        return AddressSpaceInfos[AddressSpaceIndices32Bit[index]].GetAddress();
    case 36:
        ASSERT(IsAllowed36BitType(type));
        ASSERT(IsAllowedIndexForAddress(AddressSpaceIndices36Bit[index]));
        return AddressSpaceInfos[AddressSpaceIndices36Bit[index]].GetAddress();
    case 39:
        ASSERT(IsAllowed39BitType(type));
        ASSERT(IsAllowedIndexForAddress(AddressSpaceIndices39Bit[index]));
        return AddressSpaceInfos[AddressSpaceIndices39Bit[index]].GetAddress();
    }
    UNREACHABLE();
}

std::size_t AddressSpaceInfo::GetAddressSpaceSize(std::size_t width, AddressSpaceInfo::Type type) {
    const std::size_t index{static_cast<std::size_t>(type)};
    switch (width) {
    case 32:
        ASSERT(IsAllowed32BitType(type));
        return AddressSpaceInfos[AddressSpaceIndices32Bit[index]].GetSize();
    case 36:
        ASSERT(IsAllowed36BitType(type));
        return AddressSpaceInfos[AddressSpaceIndices36Bit[index]].GetSize();
    case 39:
        ASSERT(IsAllowed39BitType(type));
        return AddressSpaceInfos[AddressSpaceIndices39Bit[index]].GetSize();
    }
    UNREACHABLE();
}

} // namespace Kernel::Memory
