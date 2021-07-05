// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <bit>
#include "common/common_types.h"

namespace Tegra::Texture::ASTC {

enum class IntegerEncoding { JustBits, Quint, Trit };

struct IntegerEncodedValue {
    constexpr IntegerEncodedValue() = default;

    constexpr IntegerEncodedValue(IntegerEncoding encoding_, u32 num_bits_)
        : encoding{encoding_}, num_bits{num_bits_} {}

    constexpr bool MatchesEncoding(const IntegerEncodedValue& other) const {
        return encoding == other.encoding && num_bits == other.num_bits;
    }

    // Returns the number of bits required to encode num_vals values.
    u32 GetBitLength(u32 num_vals) const {
        u32 total_bits = num_bits * num_vals;
        if (encoding == IntegerEncoding::Trit) {
            total_bits += (num_vals * 8 + 4) / 5;
        } else if (encoding == IntegerEncoding::Quint) {
            total_bits += (num_vals * 7 + 2) / 3;
        }
        return total_bits;
    }

    IntegerEncoding encoding{};
    u32 num_bits = 0;
    u32 bit_value = 0;
    union {
        u32 quint_value = 0;
        u32 trit_value;
    };
};

// Returns a new instance of this struct that corresponds to the
// can take no more than mav_value values
constexpr IntegerEncodedValue CreateEncoding(u32 mav_value) {
    while (mav_value > 0) {
        u32 check = mav_value + 1;

        // Is mav_value a power of two?
        if (!(check & (check - 1))) {
            return IntegerEncodedValue(IntegerEncoding::JustBits, std::popcount(mav_value));
        }

        // Is mav_value of the type 3*2^n - 1?
        if ((check % 3 == 0) && !((check / 3) & ((check / 3) - 1))) {
            return IntegerEncodedValue(IntegerEncoding::Trit, std::popcount(check / 3 - 1));
        }

        // Is mav_value of the type 5*2^n - 1?
        if ((check % 5 == 0) && !((check / 5) & ((check / 5) - 1))) {
            return IntegerEncodedValue(IntegerEncoding::Quint, std::popcount(check / 5 - 1));
        }

        // Apparently it can't be represented with a bounded integer sequence...
        // just iterate.
        mav_value--;
    }
    return IntegerEncodedValue(IntegerEncoding::JustBits, 0);
}

constexpr std::array<IntegerEncodedValue, 256> MakeEncodedValues() {
    std::array<IntegerEncodedValue, 256> encodings{};
    for (std::size_t i = 0; i < encodings.size(); ++i) {
        encodings[i] = CreateEncoding(static_cast<u32>(i));
    }
    return encodings;
}

constexpr std::array<IntegerEncodedValue, 256> ASTC_ENCODINGS_VALUES = MakeEncodedValues();

void Decompress(std::span<const uint8_t> data, uint32_t width, uint32_t height, uint32_t depth,
                uint32_t block_width, uint32_t block_height, std::span<uint8_t> output);

} // namespace Tegra::Texture::ASTC
