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

std::string GetTimeZoneString() {
    static constexpr std::array<const char*, 46> timezones{{
        "auto",      "default",   "CET", "CST6CDT", "Cuba",    "EET",    "Egypt",     "Eire",
        "EST",       "EST5EDT",   "GB",  "GB-Eire", "GMT",     "GMT+0",  "GMT-0",     "GMT0",
        "Greenwich", "Hongkong",  "HST", "Iceland", "Iran",    "Israel", "Jamaica",   "Japan",
        "Kwajalein", "Libya",     "MET", "MST",     "MST7MDT", "Navajo", "NZ",        "NZ-CHAT",
        "Poland",    "Portugal",  "PRC", "PST8PDT", "ROC",     "ROK",    "Singapore", "Turkey",
        "UCT",       "Universal", "UTC", "W-SU",    "WET",     "Zulu",
    }};

    ASSERT(Settings::values.time_zone_index < timezones.size());

    return timezones[Settings::values.time_zone_index];
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
    LogSetting("System_UseDockedMode", Settings::values.use_docked_mode);
    LogSetting("System_RngSeed", Settings::values.rng_seed.value_or(0));
    LogSetting("System_CurrentUser", Settings::values.current_user);
    LogSetting("System_LanguageIndex", Settings::values.language_index);
    LogSetting("System_RegionIndex", Settings::values.region_index);
    LogSetting("System_TimeZoneIndex", Settings::values.time_zone_index);
    LogSetting("Core_UseMultiCore", Settings::values.use_multi_core);
    LogSetting("Renderer_UseResolutionFactor", Settings::values.resolution_factor);
    LogSetting("Renderer_UseFrameLimit", Settings::values.use_frame_limit);
    LogSetting("Renderer_FrameLimit", Settings::values.frame_limit);
    LogSetting("Renderer_UseDiskShaderCache", Settings::values.use_disk_shader_cache);
    LogSetting("Renderer_GPUAccuracyLevel", Settings::values.gpu_accuracy);
    LogSetting("Renderer_UseAsynchronousGpuEmulation",
               Settings::values.use_asynchronous_gpu_emulation);
    LogSetting("Renderer_UseVsync", Settings::values.use_vsync);
    LogSetting("Renderer_UseAssemblyShaders", Settings::values.use_assembly_shaders);
    LogSetting("Renderer_AnisotropicFilteringLevel", Settings::values.max_anisotropy);
    LogSetting("Audio_OutputEngine", Settings::values.sink_id);
    LogSetting("Audio_EnableAudioStretching", Settings::values.enable_audio_stretching);
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
    return values.volume;
}

bool IsGPULevelExtreme() {
    return values.gpu_accuracy == GPUAccuracy::Extreme;
}

bool IsGPULevelHigh() {
    return values.gpu_accuracy == GPUAccuracy::Extreme || values.gpu_accuracy == GPUAccuracy::High;
}

} // namespace Settings
