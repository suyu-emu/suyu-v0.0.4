// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "video_core/rasterizer_interface.h"

namespace VideoCore {

// Optimized rasterizers implement the full rasterizer interface.
using OptimizedRasterizer = RasterizerInterface;

} // namespace VideoCore
