// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <limits>
#include <random>

#include "common/random.h"

namespace Common::Random {

[[nodiscard]] static std::random_device& GetGlobalRandomDevice() noexcept {
    static std::random_device g_random_device{};
    return g_random_device;
}

u32 Random32() {
    return GetGlobalRandomDevice()();
}

u64 Random64() {
    std::mt19937_64 gen(GetGlobalRandomDevice()());
    std::uniform_int_distribution<u64> distribution(1, std::numeric_limits<u64>::max());
    return distribution(gen);
}

std::mt19937& GetMT19937() {
    static thread_local std::mt19937 gen{GetGlobalRandomDevice()()};
    return gen;
}

} // namespace Common::Random
