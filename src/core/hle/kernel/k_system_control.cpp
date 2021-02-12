// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <random>

#include "core/hle/kernel/k_system_control.h"

namespace Kernel {

namespace {
template <typename F>
u64 GenerateUniformRange(u64 min, u64 max, F f) {
    // Handle the case where the difference is too large to represent.
    if (max == std::numeric_limits<u64>::max() && min == std::numeric_limits<u64>::min()) {
        return f();
    }

    // Iterate until we get a value in range.
    const u64 range_size = ((max + 1) - min);
    const u64 effective_max = (std::numeric_limits<u64>::max() / range_size) * range_size;
    while (true) {
        if (const u64 rnd = f(); rnd < effective_max) {
            return min + (rnd % range_size);
        }
    }
}

} // Anonymous namespace

u64 KSystemControl::GenerateRandomU64() {
    static std::random_device device;
    static std::mt19937 gen(device());
    static std::uniform_int_distribution<u64> distribution(1, std::numeric_limits<u64>::max());
    return distribution(gen);
}

u64 KSystemControl::GenerateRandomRange(u64 min, u64 max) {
    return GenerateUniformRange(min, max, GenerateRandomU64);
}

} // namespace Kernel
