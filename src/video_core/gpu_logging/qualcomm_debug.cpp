// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/gpu_logging/qualcomm_debug.h"
#include "common/logging/log.h"

namespace GPU::Logging::Qualcomm {

bool QualcommDebugger::is_initialized = false;

void QualcommDebugger::Initialize() {
    if (is_initialized) {
        return;
    }

    is_initialized = true;
    LOG_INFO(Render_Vulkan, "[Qualcomm Debug] Initialized (stub)");
}

std::string QualcommDebugger::GetDebugInfo() {
    // Stub for future Qualcomm proprietary driver debug extension support
    // This requires libadrenotools integration and Qualcomm-specific APIs
    return "Qualcomm debug info not yet implemented";
}

} // namespace GPU::Logging::Qualcomm
