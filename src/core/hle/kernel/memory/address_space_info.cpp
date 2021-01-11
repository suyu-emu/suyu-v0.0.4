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
   { .bit_width = 32, .address = Size_2_MB  , .size = Size_1_GB   - Size_2_MB  , .type = AddressSpaceInfo::Type::Is32Bit,    },
   { .bit_width = 32, .address = Size_1_GB  , .size = Size_4_GB   - Size_1_GB  , .type = AddressSpaceInfo::Type::Small64Bit, },
   { .bit_width = 32, .address = Invalid    , .size = Size_1_GB                , .type = AddressSpaceInfo::Type::Heap,       },
   { .bit_width = 32, .address = Invalid    , .size = Size_1_GB                , .type = AddressSpaceInfo::Type::Alias,      },
   { .bit_width = 36, .address = Size_128_MB, .size = Size_2_GB   - Size_128_MB, .type = AddressSpaceInfo::Type::Is32Bit,    },
   { .bit_width = 36, .address = Size_2_GB  , .size = Size_64_GB  - Size_2_GB  , .type = AddressSpaceInfo::Type::Small64Bit, },
   { .bit_width = 36, .address = Invalid    , .size = Size_6_GB                , .type = AddressSpaceInfo::Type::Heap,       },
   { .bit_width = 36, .address = Invalid    , .size = Size_6_GB                , .type = AddressSpaceInfo::Type::Alias,      },
   { .bit_width = 39, .address = Size_128_MB, .size = Size_512_GB - Size_128_MB, .type = AddressSpaceInfo::Type::Large64Bit, },
   { .bit_width = 39, .address = Invalid    , .size = Size_64_GB               , .type = AddressSpaceInfo::Type::Is32Bit     },
   { .bit_width = 39, .address = Invalid    , .size = Size_6_GB                , .type = AddressSpaceInfo::Type::Heap,       },
   { .bit_width = 39, .address = Invalid    , .size = Size_64_GB               , .type = AddressSpaceInfo::Type::Alias,      },
   { .bit_width = 39, .address = Invalid    , .size = Size_2_GB                , .type = AddressSpaceInfo::Type::Stack,      },
}};
// clang-format on

constexpr bool IsAllowedIndexForAddress(std::size_t index) {
    return index < AddressSpaceInfos.size() && AddressSpaceInfos[index].address != Invalid;
}

using IndexArray = std::array<std::size_t, static_cast<std::size_t>(AddressSpaceInfo::Type::Count)>;

constexpr IndexArray AddressSpaceIndices32Bit{
    0, 1, 0, 2, 0, 3,
};

constexpr IndexArray AddressSpaceIndices36Bit{
    4, 5, 4, 6, 4, 7,
};

constexpr IndexArray AddressSpaceIndices39Bit{
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

u64 AddressSpaceInfo::GetAddressSpaceStart(std::size_t width, Type type) {
    const std::size_t index{static_cast<std::size_t>(type)};
    switch (width) {
    case 32:
        ASSERT(IsAllowed32BitType(type));
        ASSERT(IsAllowedIndexForAddress(AddressSpaceIndices32Bit[index]));
        return AddressSpaceInfos[AddressSpaceIndices32Bit[index]].address;
    case 36:
        ASSERT(IsAllowed36BitType(type));
        ASSERT(IsAllowedIndexForAddress(AddressSpaceIndices36Bit[index]));
        return AddressSpaceInfos[AddressSpaceIndices36Bit[index]].address;
    case 39:
        ASSERT(IsAllowed39BitType(type));
        ASSERT(IsAllowedIndexForAddress(AddressSpaceIndices39Bit[index]));
        return AddressSpaceInfos[AddressSpaceIndices39Bit[index]].address;
    }
    UNREACHABLE();
    return 0;
}

std::size_t AddressSpaceInfo::GetAddressSpaceSize(std::size_t width, Type type) {
    const std::size_t index{static_cast<std::size_t>(type)};
    switch (width) {
    case 32:
        ASSERT(IsAllowed32BitType(type));
        return AddressSpaceInfos[AddressSpaceIndices32Bit[index]].size;
    case 36:
        ASSERT(IsAllowed36BitType(type));
        return AddressSpaceInfos[AddressSpaceIndices36Bit[index]].size;
    case 39:
        ASSERT(IsAllowed39BitType(type));
        return AddressSpaceInfos[AddressSpaceIndices39Bit[index]].size;
    }
    UNREACHABLE();
    return 0;
}

} // namespace Kernel::Memory
