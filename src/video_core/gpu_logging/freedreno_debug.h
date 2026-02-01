// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifdef ANDROID

#include <string>

namespace GPU::Logging::Freedreno {

class FreedrenoDebugger {
public:
    // Initialize Freedreno debugging
    static void Initialize();

    // Set TU_DEBUG environment variable flags
    static void SetTUDebugFlags(const std::string& flags);

    // Enable command stream dump
    static void EnableCommandStreamDump(bool frames_only = false);

    // Get breadcrumb information (if available)
    static std::string GetBreadcrumbs();

private:
    static bool is_initialized;
};

} // namespace GPU::Logging::Freedreno

#endif // ANDROID
