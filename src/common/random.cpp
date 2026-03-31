// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <random>
#include "common/random.h"

static std::random_device g_random_device;

namespace Common::Random {
    [[nodiscard]] u32 Random32(u32 seed) noexcept {
        return g_random_device();
    }
    [[nodiscard]] u64 Random64(u64 seed) noexcept {
        return g_random_device();
    }
    [[nodiscard]] std::mt19937 GetMT19937() noexcept {
        return std::mt19937(g_random_device());
    }
}
