// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/gpu_logging/gpu_state_capture.h"
#include <fmt/format.h>

namespace GPU::Logging {

GPUStateSnapshot GPUStateCapture::CaptureState() {
    return GPULogger::GetInstance().GetCurrentSnapshot();
}

std::string GPUStateCapture::SerializeState(const GPUStateSnapshot& snapshot) {
    std::string result;

    result += "=== GPU STATE SNAPSHOT ===\n\n";

    result += fmt::format("Driver: {}\n", static_cast<int>(snapshot.driver_type));
    result += fmt::format("Recent Calls: {}\n\n", snapshot.recent_calls.size());

    result += "=== RECENT VULKAN CALLS ===\n";
    for (const auto& call : snapshot.recent_calls) {
        result += fmt::format("{}: {}({}) -> {}\n", call.timestamp.count(), call.call_name,
                             call.parameters, call.result);
    }

    result += "\n=== MEMORY STATUS ===\n";
    result += snapshot.memory_status;

    result += "\n=== PIPELINE STATE ===\n";
    result += snapshot.pipeline_state;

    result += "\n=== DRIVER DEBUG INFO ===\n";
    result += snapshot.driver_debug_info;

    return result;
}

void GPUStateCapture::WriteCrashDump(const std::string& crash_reason) {
    GPULogger::GetInstance().DumpStateToFile(crash_reason);
}

} // namespace GPU::Logging
