// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/game_settings.h"

#include <algorithm>
#include <cctype>

#include "common/logging/log.h"
#include "common/settings.h"
#include "video_core/renderer_base.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace Core::GameSettings {

static GPUVendor GetGPU(const std::string& gpu_vendor_string) {
    struct Entry { const char* name; GPUVendor vendor; };
    static constexpr Entry GpuVendor[] = {
        // NVIDIA
        {"NVIDIA",   GPUVendor::Nvidia},
        {"Nouveau",  GPUVendor::Nvidia},
        {"NVK",      GPUVendor::Nvidia},
        {"Tegra",    GPUVendor::Nvidia},
        // AMD
        {"AMD",       GPUVendor::AMD},
        {"RadeonSI",  GPUVendor::AMD},
        {"RADV",      GPUVendor::AMD},
        {"AMDVLK",    GPUVendor::AMD},
        {"R600",      GPUVendor::AMD},
        // Intel
        {"Intel",     GPUVendor::Intel},
        {"ANV",       GPUVendor::Intel},
        {"i965",      GPUVendor::Intel},
        {"i915",      GPUVendor::Intel},
        {"OpenSWR",   GPUVendor::Intel},
        // Apple
        {"Apple",     GPUVendor::Apple},
        {"MoltenVK",  GPUVendor::Apple},
        // Qualcomm / Adreno
        {"Qualcomm",  GPUVendor::Qualcomm},
        {"Turnip",    GPUVendor::Qualcomm},
        // ARM / Mali
        {"Mali",      GPUVendor::ARM},
        {"PanVK",     GPUVendor::ARM},
        // Imagination / PowerVR
        {"PowerVR",   GPUVendor::Imagination},
        {"PVR",       GPUVendor::Imagination},
        // Microsoft / WARP / D3D12 GL
        {"D3D12",     GPUVendor::Microsoft},
        {"Microsoft", GPUVendor::Microsoft},
        {"WARP",      GPUVendor::Microsoft},
    };

    for (const auto& entry : GpuVendor) {
        if (gpu_vendor_string == entry.name) {
            return entry.vendor;
        }
    }

    // legacy (shouldn't be needed anymore, but just in case)
    std::string gpu = gpu_vendor_string;
    std::transform(gpu.begin(), gpu.end(), gpu.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    if (gpu.find("geforce") != std::string::npos) {
        return GPUVendor::Nvidia;
    }
    if (gpu.find("radeon") != std::string::npos || gpu.find("ati") != std::string::npos) {
        return GPUVendor::AMD;
    }

    return GPUVendor::Unknown;
}

static OS DetectOS() {
#if defined(_WIN32)
    return OS::Windows;
#elif defined(__FIREOS__)
    return OS::FireOS;
#elif defined(__ANDROID__)
    return OS::Android;
#elif defined(__OHOS__)
    return OS::HarmonyOS;
#elif defined(__HAIKU__)
    return OS::HaikuOS;
#elif defined(__DragonFly__)
    return OS::DragonFlyBSD;
#elif defined(__NetBSD__)
    return OS::NetBSD;
#elif defined(__OpenBSD__)
    return OS::OpenBSD;
#elif defined(_AIX)
    return OS::AIX;
#elif defined(__managarm__)
    return OS::Managarm;
#elif defined(__redox__)
    return OS::RedoxOS;
#elif defined(__APPLE__) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    return OS::IOS;
#elif defined(__APPLE__)
    return OS::MacOS;
#elif defined(__FreeBSD__)
    return OS::FreeBSD;
#elif defined(__sun) && defined(__SVR4)
    return OS::Solaris;
#elif defined(__linux__)
    return OS::Linux;
#else
    return OS::Unknown;
#endif
}

EnvironmentInfo DetectEnvironment(const VideoCore::RendererBase& renderer) {
    EnvironmentInfo env{};
    env.os = DetectOS();
    env.vendor_string = renderer.GetDeviceVendor();
    env.vendor = GetGPU(env.vendor_string);
    return env;
}

void LoadOverrides(std::uint64_t program_id, const VideoCore::RendererBase& renderer) {
    const auto env = DetectEnvironment(renderer);

    switch (static_cast<TitleID>(program_id)) {
        case TitleID::NinjaGaidenRagebound:
            Settings::values.use_squashed_iterated_blend = true;
            break;
        default:
            break;
    }

    LOG_INFO(Core, "Applied game settings for title ID {:016X} on OS {}, GPU vendor {} ({})",
             program_id,
             static_cast<int>(env.os),
             static_cast<int>(env.vendor),
             env.vendor_string);
}

} // namespace Core::GameSettings
