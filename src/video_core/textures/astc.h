// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdint>

namespace Tegra::Texture::ASTC {

/// Count the number of bits set in a number.
constexpr u32 Popcnt(u32 n) {
    u32 c = 0;
    for (; n; c++) {
        n &= n - 1;
    }
    return c;
}

enum class IntegerEncoding { JustBits, Qus32, Trit };

struct IntegerEncodedValue {
    constexpr IntegerEncodedValue() = default;

    constexpr IntegerEncodedValue(IntegerEncoding encoding_, u32 num_bits_)
        : encoding{encoding_}, num_bits{num_bits_} {}

    constexpr bool MatchesEncoding(const IntegerEncodedValue& other) const {
        return encoding == other.encoding && num_bits == other.num_bits;
    }

    // Returns the number of bits required to encode nVals values.
    u32 GetBitLength(u32 nVals) const {
        u32 totalBits = num_bits * nVals;
        if (encoding == IntegerEncoding::Trit) {
            totalBits += (nVals * 8 + 4) / 5;
        } else if (encoding == IntegerEncoding::Qus32) {
            totalBits += (nVals * 7 + 2) / 3;
        }
        return totalBits;
    }

    IntegerEncoding encoding{};
    u32 num_bits = 0;
    u32 bit_value = 0;
    union {
        u32 qus32_value = 0;
        u32 trit_value;
    };
};

// Returns a new instance of this struct that corresponds to the
// can take no more than maxval values
static constexpr IntegerEncodedValue CreateEncoding(u32 maxVal) {
    while (maxVal > 0) {
        u32 check = maxVal + 1;

        // Is maxVal a power of two?
        if (!(check & (check - 1))) {
            return IntegerEncodedValue(IntegerEncoding::JustBits, Popcnt(maxVal));
        }

        // Is maxVal of the type 3*2^n - 1?
        if ((check % 3 == 0) && !((check / 3) & ((check / 3) - 1))) {
            return IntegerEncodedValue(IntegerEncoding::Trit, Popcnt(check / 3 - 1));
        }

        // Is maxVal of the type 5*2^n - 1?
        if ((check % 5 == 0) && !((check / 5) & ((check / 5) - 1))) {
            return IntegerEncodedValue(IntegerEncoding::Qus32, Popcnt(check / 5 - 1));
        }

        // Apparently it can't be represented with a bounded integer sequence...
        // just iterate.
        maxVal--;
    }
    return IntegerEncodedValue(IntegerEncoding::JustBits, 0);
}

static constexpr std::array<IntegerEncodedValue, 256> MakeEncodedValues() {
    std::array<IntegerEncodedValue, 256> encodings{};
    for (std::size_t i = 0; i < encodings.size(); ++i) {
        encodings[i] = CreateEncoding(static_cast<u32>(i));
    }
    return encodings;
}

static constexpr std::array<IntegerEncodedValue, 256> EncodingsValues = MakeEncodedValues();

// Replicates low numBits such that [(toBit - 1):(toBit - 1 - fromBit)]
// is the same as [(numBits - 1):0] and repeats all the way down.
template <typename IntType>
static constexpr IntType Replicate(IntType val, u32 numBits, u32 toBit) {
    if (numBits == 0) {
        return 0;
    }
    if (toBit == 0) {
        return 0;
    }
    const IntType v = val & static_cast<IntType>((1 << numBits) - 1);
    IntType res = v;
    u32 reslen = numBits;
    while (reslen < toBit) {
        u32 comp = 0;
        if (numBits > toBit - reslen) {
            u32 newshift = toBit - reslen;
            comp = numBits - newshift;
            numBits = newshift;
        }
        res = static_cast<IntType>(res << numBits);
        res = static_cast<IntType>(res | (v >> comp));
        reslen += numBits;
    }
    return res;
}

static constexpr std::size_t NumReplicateEntries(u32 num_bits) {
    return std::size_t(1) << num_bits;
}

template <typename IntType, u32 num_bits, u32 to_bit>
static constexpr auto MakeReplicateTable() {
    std::array<IntType, NumReplicateEntries(num_bits)> table{};
    for (IntType value = 0; value < static_cast<IntType>(std::size(table)); ++value) {
        table[value] = Replicate(value, num_bits, to_bit);
    }
    return table;
}

static constexpr auto REPLICATE_BYTE_TO_16_TABLE = MakeReplicateTable<u32, 8, 16>();
static constexpr u32 ReplicateByteTo16(std::size_t value) {
    return REPLICATE_BYTE_TO_16_TABLE[value];
}

static constexpr auto REPLICATE_BIT_TO_7_TABLE = MakeReplicateTable<u32, 1, 7>();
static constexpr u32 ReplicateBitTo7(std::size_t value) {
    return REPLICATE_BIT_TO_7_TABLE[value];
}

static constexpr auto REPLICATE_BIT_TO_9_TABLE = MakeReplicateTable<u32, 1, 9>();
static constexpr u32 ReplicateBitTo9(std::size_t value) {
    return REPLICATE_BIT_TO_9_TABLE[value];
}

static constexpr auto REPLICATE_1_BIT_TO_8_TABLE = MakeReplicateTable<u32, 1, 8>();
static constexpr auto REPLICATE_2_BIT_TO_8_TABLE = MakeReplicateTable<u32, 2, 8>();
static constexpr auto REPLICATE_3_BIT_TO_8_TABLE = MakeReplicateTable<u32, 3, 8>();
static constexpr auto REPLICATE_4_BIT_TO_8_TABLE = MakeReplicateTable<u32, 4, 8>();
static constexpr auto REPLICATE_5_BIT_TO_8_TABLE = MakeReplicateTable<u32, 5, 8>();
static constexpr auto REPLICATE_6_BIT_TO_8_TABLE = MakeReplicateTable<u32, 6, 8>();
static constexpr auto REPLICATE_7_BIT_TO_8_TABLE = MakeReplicateTable<u32, 7, 8>();
static constexpr auto REPLICATE_8_BIT_TO_8_TABLE = MakeReplicateTable<u32, 8, 8>();
/// Use a precompiled table with the most common usages, if it's not in the expected range, fallback
/// to the runtime implementation
static constexpr u32 FastReplicateTo8(u32 value, u32 num_bits) {
    switch (num_bits) {
    case 1:
        return REPLICATE_1_BIT_TO_8_TABLE[value];
    case 2:
        return REPLICATE_2_BIT_TO_8_TABLE[value];
    case 3:
        return REPLICATE_3_BIT_TO_8_TABLE[value];
    case 4:
        return REPLICATE_4_BIT_TO_8_TABLE[value];
    case 5:
        return REPLICATE_5_BIT_TO_8_TABLE[value];
    case 6:
        return REPLICATE_6_BIT_TO_8_TABLE[value];
    case 7:
        return REPLICATE_7_BIT_TO_8_TABLE[value];
    case 8:
        return REPLICATE_8_BIT_TO_8_TABLE[value];
    default:
        return Replicate(value, num_bits, 8);
    }
}

static constexpr auto REPLICATE_1_BIT_TO_6_TABLE = MakeReplicateTable<u32, 1, 6>();
static constexpr auto REPLICATE_2_BIT_TO_6_TABLE = MakeReplicateTable<u32, 2, 6>();
static constexpr auto REPLICATE_3_BIT_TO_6_TABLE = MakeReplicateTable<u32, 3, 6>();
static constexpr auto REPLICATE_4_BIT_TO_6_TABLE = MakeReplicateTable<u32, 4, 6>();
static constexpr auto REPLICATE_5_BIT_TO_6_TABLE = MakeReplicateTable<u32, 5, 6>();

static constexpr u32 FastReplicateTo6(u32 value, u32 num_bits) {
    switch (num_bits) {
    case 1:
        return REPLICATE_1_BIT_TO_6_TABLE[value];
    case 2:
        return REPLICATE_2_BIT_TO_6_TABLE[value];
    case 3:
        return REPLICATE_3_BIT_TO_6_TABLE[value];
    case 4:
        return REPLICATE_4_BIT_TO_6_TABLE[value];
    case 5:
        return REPLICATE_5_BIT_TO_6_TABLE[value];
    default:
        return Replicate(value, num_bits, 6);
    }
}

void Decompress(std::span<const uint8_t> data, uint32_t width, uint32_t height, uint32_t depth,
                uint32_t block_width, uint32_t block_height, std::span<uint8_t> output);

} // namespace Tegra::Texture::ASTC
