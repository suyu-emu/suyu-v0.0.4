// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2021 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <string>
#include <vector>

#include "dynarmic/common/common_types.h"

namespace Dynarmic::Common {

void DumpDisassembledX64(const void* ptr, size_t size);
/**
 * Disassemble `size' bytes from `ptr' and return the disassembled lines as a vector
 * of strings.
 */
std::vector<std::string> DisassembleX64(const void* ptr, size_t size);
}  // namespace Dynarmic::Common
