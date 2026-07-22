// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <string>

#include "dynarmic/common/common_types.h"

namespace Dynarmic::A32 {

std::string DisassembleArm(u32 instruction);
std::string DisassembleThumb16(u16 instruction);

}  // namespace Dynarmic::A32
