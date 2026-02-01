// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef ANDROID

#include "video_core/gpu_logging/freedreno_debug.h"
#include "common/logging/log.h"

#include <cstdlib>

namespace GPU::Logging::Freedreno {

bool FreedrenoDebugger::is_initialized = false;

void FreedrenoDebugger::Initialize() {
    if (is_initialized) {
        return;
    }

    is_initialized = true;
    LOG_INFO(Render_Vulkan, "[Freedreno Debug] Initialized");
}

void FreedrenoDebugger::SetTUDebugFlags(const std::string& flags) {
    if (flags.empty()) {
        return;
    }

    // Set TU_DEBUG environment variable
    // Note: This should be set BEFORE Vulkan driver is loaded
    setenv("TU_DEBUG", flags.c_str(), 1);

    LOG_INFO(Render_Vulkan, "[Freedreno Debug] TU_DEBUG set to: {}", flags);
}

void FreedrenoDebugger::EnableCommandStreamDump(bool frames_only) {
    // Enable FD_RD_DUMP for command stream capture
    const char* dump_flags = frames_only ? "frames" : "all";
    setenv("FD_RD_DUMP", dump_flags, 1);

    LOG_INFO(Render_Vulkan, "[Freedreno Debug] Command stream dump enabled: {}", dump_flags);
}

std::string FreedrenoDebugger::GetBreadcrumbs() {
    // Breadcrumb reading requires driver-specific implementation
    // This is a stub for future implementation
    return "Breadcrumb capture not yet implemented";
}

} // namespace GPU::Logging::Freedreno

#endif // ANDROID
