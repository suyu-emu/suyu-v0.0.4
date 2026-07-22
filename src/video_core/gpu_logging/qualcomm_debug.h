// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

namespace GPU::Logging::Qualcomm {

class QualcommDebugger {
public:
    // Initialize Qualcomm debugging (stub for future implementation)
    static void Initialize();

    // Get debug information from Qualcomm driver
    static std::string GetDebugInfo();

private:
    static bool is_initialized;
};

} // namespace GPU::Logging::Qualcomm
