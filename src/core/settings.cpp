// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/file_util.h"
#include "core/core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/service/hid/hid.h"
#include "core/settings.h"
#include "video_core/renderer_base.h"

namespace Settings {

namespace NativeButton {
const std::array<const char*, NumButtons> mapping = {{
    "button_a",
    "button_b",
    "button_x",
    "button_y",
    "button_lstick",
    "button_rstick",
    "button_l",
    "button_r",
    "button_zl",
    "button_zr",
    "button_plus",
    "button_minus",
    "button_dleft",
    "button_dup",
    "button_dright",
    "button_ddown",
    "button_lstick_left",
    "button_lstick_up",
    "button_lstick_right",
    "button_lstick_down",
    "button_rstick_left",
    "button_rstick_up",
    "button_rstick_right",
    "button_rstick_down",
    "button_sl",
    "button_sr",
    "button_home",
    "button_screenshot",
}};
}

namespace NativeAnalog {
const std::array<const char*, NumAnalogs> mapping = {{
    "lstick",
    "rstick",
}};
}

namespace NativeMouseButton {
const std::array<const char*, NumMouseButtons> mapping = {{
    "left",
    "right",
    "middle",
    "forward",
    "back",
}};
}

Values values = {};
bool configuring_global = true;

std::string GetTimeZoneString() {
    static constexpr std::array<const char*, 46> timezones{{
        "auto",      "default",   "CET", "CST6CDT", "Cuba",    "EET",    "Egypt",     "Eire",
        "EST",       "EST5EDT",   "GB",  "GB-Eire", "GMT",     "GMT+0",  "GMT-0",     "GMT0",
        "Greenwich", "Hongkong",  "HST", "Iceland", "Iran",    "Israel", "Jamaica",   "Japan",
        "Kwajalein", "Libya",     "MET", "MST",     "MST7MDT", "Navajo", "NZ",        "NZ-CHAT",
        "Poland",    "Portugal",  "PRC", "PST8PDT", "ROC",     "ROK",    "Singapore", "Turkey",
        "UCT",       "Universal", "UTC", "W-SU",    "WET",     "Zulu",
    }};

    ASSERT(Settings::values.time_zone_index.GetValue() < timezones.size());

    return timezones[Settings::values.time_zone_index.GetValue()];
}

void Apply() {
    GDBStub::SetServerPort(values.gdbstub_port);
    GDBStub::ToggleServer(values.use_gdbstub);

    auto& system_instance = Core::System::GetInstance();
    if (system_instance.IsPoweredOn()) {
        system_instance.Renderer().RefreshBaseSettings();
    }

    Service::HID::ReloadInputDevices();
}

template <typename T>
void LogSetting(const std::string& name, const T& value) {
    LOG_INFO(Config, "{}: {}", name, value);
}

void LogSettings() {
    LOG_INFO(Config, "yuzu Configuration:");
    LogSetting("Controls_UseDockedMode", Settings::values.use_docked_mode);
    LogSetting("System_RngSeed", Settings::values.rng_seed.GetValue().value_or(0));
    LogSetting("System_CurrentUser", Settings::values.current_user);
    LogSetting("System_LanguageIndex", Settings::values.language_index.GetValue());
    LogSetting("System_RegionIndex", Settings::values.region_index.GetValue());
    LogSetting("System_TimeZoneIndex", Settings::values.time_zone_index.GetValue());
    LogSetting("Core_UseMultiCore", Settings::values.use_multi_core.GetValue());
    LogSetting("Renderer_UseResolutionFactor", Settings::values.resolution_factor.GetValue());
    LogSetting("Renderer_UseFrameLimit", Settings::values.use_frame_limit.GetValue());
    LogSetting("Renderer_FrameLimit", Settings::values.frame_limit.GetValue());
    LogSetting("Renderer_UseDiskShaderCache", Settings::values.use_disk_shader_cache.GetValue());
    LogSetting("Renderer_GPUAccuracyLevel", Settings::values.gpu_accuracy.GetValue());
    LogSetting("Renderer_UseAsynchronousGpuEmulation",
               Settings::values.use_asynchronous_gpu_emulation.GetValue());
    LogSetting("Renderer_UseVsync", Settings::values.use_vsync.GetValue());
    LogSetting("Renderer_UseAssemblyShaders", Settings::values.use_assembly_shaders.GetValue());
    LogSetting("Renderer_AnisotropicFilteringLevel", Settings::values.max_anisotropy.GetValue());
    LogSetting("Audio_OutputEngine", Settings::values.sink_id);
    LogSetting("Audio_EnableAudioStretching", Settings::values.enable_audio_stretching.GetValue());
    LogSetting("Audio_OutputDevice", Settings::values.audio_device_id);
    LogSetting("DataStorage_UseVirtualSd", Settings::values.use_virtual_sd);
    LogSetting("DataStorage_NandDir", FileUtil::GetUserPath(FileUtil::UserPath::NANDDir));
    LogSetting("DataStorage_SdmcDir", FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir));
    LogSetting("Debugging_UseGdbstub", Settings::values.use_gdbstub);
    LogSetting("Debugging_GdbstubPort", Settings::values.gdbstub_port);
    LogSetting("Debugging_ProgramArgs", Settings::values.program_args);
    LogSetting("Services_BCATBackend", Settings::values.bcat_backend);
    LogSetting("Services_BCATBoxcatLocal", Settings::values.bcat_boxcat_local);
}

float Volume() {
    if (values.audio_muted) {
        return 0.0f;
    }
    return values.volume.GetValue();
}

bool IsGPULevelExtreme() {
    return values.gpu_accuracy.GetValue() == GPUAccuracy::Extreme;
}

bool IsGPULevelHigh() {
    return values.gpu_accuracy.GetValue() == GPUAccuracy::Extreme ||
           values.gpu_accuracy.GetValue() == GPUAccuracy::High;
}

void RestoreGlobalState() {
    // If a game is running, DO NOT restore the global settings state
    if (Core::System::GetInstance().IsPoweredOn()) {
        return;
    }

    // Audio
    values.enable_audio_stretching.SetGlobal(true);
    values.volume.SetGlobal(true);

    // Core
    values.use_multi_core.SetGlobal(true);

    // Renderer
    values.renderer_backend.SetGlobal(true);
    values.vulkan_device.SetGlobal(true);
    values.aspect_ratio.SetGlobal(true);
    values.max_anisotropy.SetGlobal(true);
    values.use_frame_limit.SetGlobal(true);
    values.frame_limit.SetGlobal(true);
    values.use_disk_shader_cache.SetGlobal(true);
    values.gpu_accuracy.SetGlobal(true);
    values.use_asynchronous_gpu_emulation.SetGlobal(true);
    values.use_vsync.SetGlobal(true);
    values.use_assembly_shaders.SetGlobal(true);
    values.use_fast_gpu_time.SetGlobal(true);
    values.force_30fps_mode.SetGlobal(true);
    values.bg_red.SetGlobal(true);
    values.bg_green.SetGlobal(true);
    values.bg_blue.SetGlobal(true);

    // System
    values.language_index.SetGlobal(true);
    values.region_index.SetGlobal(true);
    values.time_zone_index.SetGlobal(true);
    values.rng_seed.SetGlobal(true);
    values.custom_rtc.SetGlobal(true);
    values.sound_index.SetGlobal(true);
}

} // namespace Settings
