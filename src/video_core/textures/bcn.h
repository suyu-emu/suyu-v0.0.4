// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <stdint.h>

namespace Tegra::Texture::BCN {

void CompressBC1(std::span<const uint8_t> data, uint32_t width, uint32_t height, uint32_t depth,
                 std::span<uint8_t> output);

void CompressBC3(std::span<const uint8_t> data, uint32_t width, uint32_t height, uint32_t depth,
                 std::span<uint8_t> output);

} // namespace Tegra::Texture::BCN
