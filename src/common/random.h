// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <random>
#include "common/common_types.h"

namespace Common::Random {

/// Generate a random 32-bit unsigned integer.
u32 Random32();

/// Generate a random 64-bit unsigned integer.
u64 Random64();

/// Get a thread-local MT19937 engine seeded from the system random device.
std::mt19937& GetMT19937();

} // namespace Common::Random
