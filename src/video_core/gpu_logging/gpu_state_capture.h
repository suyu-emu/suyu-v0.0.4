// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include "video_core/gpu_logging/gpu_logging.h"

namespace GPU::Logging {

class GPUStateCapture {
public:
    // Capture current GPU state from logging system
    static GPUStateSnapshot CaptureState();

    // Serialize state to human-readable format
    static std::string SerializeState(const GPUStateSnapshot& snapshot);

    // Write detailed crash dump (implemented in GPULogger)
    static void WriteCrashDump(const std::string& crash_reason);
};

} // namespace GPU::Logging
