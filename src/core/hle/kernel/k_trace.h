// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/literals.h"

namespace Kernel {

using namespace Common::Literals;

constexpr bool IsKTraceEnabled = false;
constexpr std::size_t KTraceBufferSize = IsKTraceEnabled ? 16_MiB : 0;

} // namespace Kernel
