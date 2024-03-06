// SPDX-FileCopyrightText: Copyright 2022 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "common/common_types.h"

namespace AudioCore::Renderer {

struct EffectResultState {
    std::array<u8, 0x80> state;
};

} // namespace AudioCore::Renderer
