// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <random>
#include "common/common_types.h"

namespace Common::Random {
    [[nodiscard]] u32 Random32(u32 seed) noexcept;
    [[nodiscard]] u64 Random64(u64 seed) noexcept;
    [[nodiscard]] std::mt19937 GetMT19937() noexcept;
}
