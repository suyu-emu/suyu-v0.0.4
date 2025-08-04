// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2018 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include "dynarmic/common/common_types.h"

namespace Dynarmic::FP {

class FPCR;
class FPSR;
enum class RoundingMode;

template<typename FPT>
u64 FPToFixed(size_t ibits, FPT op, size_t fbits, bool unsigned_, FPCR fpcr, RoundingMode rounding, FPSR& fpsr);

}  // namespace Dynarmic::FP
