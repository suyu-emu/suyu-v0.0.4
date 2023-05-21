// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#if __cpp_lib_chrono >= 201907L
#include <chrono>
#else
#include <ctime>
#include <limits>
#endif
#include <string_view>
#include <fmt/chrono.h>
#include <fmt/core.h>

#include "common/assert.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/settings.h"

namespace Settings {

Values values;
static bool configuring_global = true;

std::string GetTimeZoneString() {
    static constexpr std::array timezones{
        "GMT",       "GMT",       "CET", "CST6CDT", "Cuba",    "EET",    "Egypt",     "Eire",
        "EST",       "EST5EDT",   "GB",  "GB-Eire", "GMT",     "GMT+0",  "GMT-0",     "GMT0",
        "Greenwich", "Hongkong",  "HST", "Iceland", "Iran",    "Israel", "Jamaica",   "Japan",
        "Kwajalein", "Libya",     "MET", "MST",     "MST7MDT", "Navajo", "NZ",        "NZ-CHAT",
        "Poland",    "Portugal",  "PRC", "PST8PDT", "ROC",     "ROK",    "Singapore", "Turkey",
        "UCT",       "Universal", "UTC", "W-SU",    "WET",     "Zulu",
    };

    const auto time_zone_index = static_cast<std::size_t>(values.time_zone_index.GetValue());
    ASSERT(time_zone_index < timezones.size());
    std::string location_name;
    switch (time_zone_index) {
    case 0: { // Auto
#if __cpp_lib_chrono >= 201907L
        const struct std::chrono::tzdb& time_zone_data = std::chrono::get_tzdb();
        const std::chrono::time_zone* current_zone = time_zone_data.current_zone();
        std::string_view current_zone_name = current_zone->name();
        location_name = current_zone_name;
#elif defined(MINGW)
        // MinGW has broken strftime -- https://sourceforge.net/p/mingw-w64/bugs/793/
        // e.g. fmt::format("{:%z}") -- returns "Eastern Daylight Time" when it should be "-0400"
        location_name = timezones[0];
        break;
#else
        static constexpr std::array offsets{
            0,     0,     3600,   -21600, -19768, 7200,   7509,  -1521,  -18000, -18000,
            -75,   -75,   0,      0,      0,      0,      0,     27402,  -36000, -968,
            12344, 8454,  -18430, 33539,  40160,  3164,   3600,  -25200, -25200, -25196,
            41944, 44028, 5040,   -2205,  29143,  -28800, 29160, 30472,  24925,  6952,
            0,     0,     0,      9017,   0,      0,
        };

        static constexpr std::array dst{
            false, false, true,  true,  true,  true,  true,  true,  false, true,  true, true,
            false, false, false, false, false, true,  false, false, true,  true,  true, true,
            false, true,  true,  false, true,  true,  true,  true,  true,  true,  true, true,
            true,  true,  true,  true,  false, false, false, true,  true,  false,
        };

        const auto now = std::time(nullptr);
        const struct std::tm& local = *std::localtime(&now);
        const std::string clock_offset_s = fmt::format("{:%z}", local);
        if (clock_offset_s.empty()) {
            location_name = timezones[0];
            break;
        }
        const int hours_offset = std::stoi(clock_offset_s) / 100;
        const int minutes_offset = std::stoi(clock_offset_s) - hours_offset * 100;
        const int system_offset =
            hours_offset * 3600 + minutes_offset * 60 - (local.tm_isdst ? 3600 : 0);

        int min = std::numeric_limits<int>::max();
        int min_index = -1;
        for (u32 i = 2; i < offsets.size(); i++) {
            // Skip if system is celebrating DST but considered time zone does not
            if (local.tm_isdst && !dst[i]) {
                continue;
            }

            const auto offset = offsets[i];
            const int difference = std::abs(std::abs(offset) - std::abs(system_offset));
            if (difference < min) {
                min = difference;
                min_index = i;
            }
        }

        location_name = timezones[min_index];
#endif
        break;
    }
    default:
        location_name = timezones[time_zone_index];
        break;
    }

    return location_name;
}

void LogSettings() {
    const auto log_setting = [](std::string_view name, const auto& value) {
        LOG_INFO(Config, "{}: {}", name, value);
    };

    const auto log_path = [](std::string_view name, const std::filesystem::path& path) {
        LOG_INFO(Config, "{}: {}", name, Common::FS::PathToUTF8String(path));
    };

    LOG_INFO(Config, "yuzu Configuration:");
    log_setting("Controls_UseDockedMode", values.use_docked_mode.GetValue());
    log_setting("System_RngSeed", values.rng_seed.GetValue().value_or(0));
    log_setting("System_DeviceName", values.device_name.GetValue());
    log_setting("System_CurrentUser", values.current_user.GetValue());
    log_setting("System_LanguageIndex", values.language_index.GetValue());
    log_setting("System_RegionIndex", values.region_index.GetValue());
    log_setting("System_TimeZoneIndex", values.time_zone_index.GetValue());
    log_setting("System_UnsafeMemoryLayout", values.use_unsafe_extended_memory_layout.GetValue());
    log_setting("Core_UseMultiCore", values.use_multi_core.GetValue());
    log_setting("CPU_Accuracy", values.cpu_accuracy.GetValue());
    log_setting("Renderer_UseResolutionScaling", values.resolution_setup.GetValue());
    log_setting("Renderer_ScalingFilter", values.scaling_filter.GetValue());
    log_setting("Renderer_FSRSlider", values.fsr_sharpening_slider.GetValue());
    log_setting("Renderer_AntiAliasing", values.anti_aliasing.GetValue());
    log_setting("Renderer_UseSpeedLimit", values.use_speed_limit.GetValue());
    log_setting("Renderer_SpeedLimit", values.speed_limit.GetValue());
    log_setting("Renderer_UseDiskShaderCache", values.use_disk_shader_cache.GetValue());
    log_setting("Renderer_GPUAccuracyLevel", values.gpu_accuracy.GetValue());
    log_setting("Renderer_UseAsynchronousGpuEmulation",
                values.use_asynchronous_gpu_emulation.GetValue());
    log_setting("Renderer_NvdecEmulation", values.nvdec_emulation.GetValue());
    log_setting("Renderer_AccelerateASTC", values.accelerate_astc.GetValue());
    log_setting("Renderer_AsyncASTC", values.async_astc.GetValue());
    log_setting("Renderer_AstcRecompression", values.astc_recompression.GetValue());
    log_setting("Renderer_UseVsync", values.vsync_mode.GetValue());
    log_setting("Renderer_UseReactiveFlushing", values.use_reactive_flushing.GetValue());
    log_setting("Renderer_ShaderBackend", values.shader_backend.GetValue());
    log_setting("Renderer_UseAsynchronousShaders", values.use_asynchronous_shaders.GetValue());
    log_setting("Renderer_AnisotropicFilteringLevel", values.max_anisotropy.GetValue());
    log_setting("Audio_OutputEngine", values.sink_id.GetValue());
    log_setting("Audio_OutputDevice", values.audio_output_device_id.GetValue());
    log_setting("Audio_InputDevice", values.audio_input_device_id.GetValue());
    log_setting("DataStorage_UseVirtualSd", values.use_virtual_sd.GetValue());
    log_path("DataStorage_CacheDir", Common::FS::GetYuzuPath(Common::FS::YuzuPath::CacheDir));
    log_path("DataStorage_ConfigDir", Common::FS::GetYuzuPath(Common::FS::YuzuPath::ConfigDir));
    log_path("DataStorage_LoadDir", Common::FS::GetYuzuPath(Common::FS::YuzuPath::LoadDir));
    log_path("DataStorage_NANDDir", Common::FS::GetYuzuPath(Common::FS::YuzuPath::NANDDir));
    log_path("DataStorage_SDMCDir", Common::FS::GetYuzuPath(Common::FS::YuzuPath::SDMCDir));
    log_setting("Debugging_ProgramArgs", values.program_args.GetValue());
    log_setting("Debugging_GDBStub", values.use_gdbstub.GetValue());
    log_setting("Input_EnableMotion", values.motion_enabled.GetValue());
    log_setting("Input_EnableVibration", values.vibration_enabled.GetValue());
    log_setting("Input_EnableTouch", values.touchscreen.enabled);
    log_setting("Input_EnableMouse", values.mouse_enabled.GetValue());
    log_setting("Input_EnableKeyboard", values.keyboard_enabled.GetValue());
    log_setting("Input_EnableRingController", values.enable_ring_controller.GetValue());
    log_setting("Input_EnableIrSensor", values.enable_ir_sensor.GetValue());
    log_setting("Input_EnableCustomJoycon", values.enable_joycon_driver.GetValue());
    log_setting("Input_EnableCustomProController", values.enable_procon_driver.GetValue());
    log_setting("Input_EnableRawInput", values.enable_raw_input.GetValue());
}

bool IsConfiguringGlobal() {
    return configuring_global;
}

void SetConfiguringGlobal(bool is_global) {
    configuring_global = is_global;
}

bool IsGPULevelExtreme() {
    return values.gpu_accuracy.GetValue() == GPUAccuracy::Extreme;
}

bool IsGPULevelHigh() {
    return values.gpu_accuracy.GetValue() == GPUAccuracy::Extreme ||
           values.gpu_accuracy.GetValue() == GPUAccuracy::High;
}

bool IsFastmemEnabled() {
    if (values.cpu_debug_mode) {
        return static_cast<bool>(values.cpuopt_fastmem);
    }
    return true;
}

float Volume() {
    if (values.audio_muted) {
        return 0.0f;
    }
    return values.volume.GetValue() / static_cast<f32>(values.volume.GetDefault());
}

void UpdateRescalingInfo() {
    const auto setup = values.resolution_setup.GetValue();
    auto& info = values.resolution_info;
    info.downscale = false;
    switch (setup) {
    case ResolutionSetup::Res1_2X:
        info.up_scale = 1;
        info.down_shift = 1;
        info.downscale = true;
        break;
    case ResolutionSetup::Res3_4X:
        info.up_scale = 3;
        info.down_shift = 2;
        info.downscale = true;
        break;
    case ResolutionSetup::Res1X:
        info.up_scale = 1;
        info.down_shift = 0;
        break;
    case ResolutionSetup::Res3_2X:
        info.up_scale = 3;
        info.down_shift = 1;
        break;
    case ResolutionSetup::Res2X:
        info.up_scale = 2;
        info.down_shift = 0;
        break;
    case ResolutionSetup::Res3X:
        info.up_scale = 3;
        info.down_shift = 0;
        break;
    case ResolutionSetup::Res4X:
        info.up_scale = 4;
        info.down_shift = 0;
        break;
    case ResolutionSetup::Res5X:
        info.up_scale = 5;
        info.down_shift = 0;
        break;
    case ResolutionSetup::Res6X:
        info.up_scale = 6;
        info.down_shift = 0;
        break;
    case ResolutionSetup::Res7X:
        info.up_scale = 7;
        info.down_shift = 0;
        break;
    case ResolutionSetup::Res8X:
        info.up_scale = 8;
        info.down_shift = 0;
        break;
    default:
        ASSERT(false);
        info.up_scale = 1;
        info.down_shift = 0;
        break;
    }
    info.up_factor = static_cast<f32>(info.up_scale) / (1U << info.down_shift);
    info.down_factor = static_cast<f32>(1U << info.down_shift) / info.up_scale;
    info.active = info.up_scale != 1 || info.down_shift != 0;
}

void RestoreGlobalState(bool is_powered_on) {
    // If a game is running, DO NOT restore the global settings state
    if (is_powered_on) {
        return;
    }

    // Audio
    values.volume.SetGlobal(true);

    // Core
    values.use_multi_core.SetGlobal(true);
    values.use_unsafe_extended_memory_layout.SetGlobal(true);

    // CPU
    values.cpu_accuracy.SetGlobal(true);
    values.cpuopt_unsafe_unfuse_fma.SetGlobal(true);
    values.cpuopt_unsafe_reduce_fp_error.SetGlobal(true);
    values.cpuopt_unsafe_ignore_standard_fpcr.SetGlobal(true);
    values.cpuopt_unsafe_inaccurate_nan.SetGlobal(true);
    values.cpuopt_unsafe_fastmem_check.SetGlobal(true);
    values.cpuopt_unsafe_ignore_global_monitor.SetGlobal(true);

    // Renderer
    values.fsr_sharpening_slider.SetGlobal(true);
    values.renderer_backend.SetGlobal(true);
    values.async_presentation.SetGlobal(true);
    values.renderer_force_max_clock.SetGlobal(true);
    values.vulkan_device.SetGlobal(true);
    values.fullscreen_mode.SetGlobal(true);
    values.aspect_ratio.SetGlobal(true);
    values.resolution_setup.SetGlobal(true);
    values.scaling_filter.SetGlobal(true);
    values.anti_aliasing.SetGlobal(true);
    values.max_anisotropy.SetGlobal(true);
    values.use_speed_limit.SetGlobal(true);
    values.speed_limit.SetGlobal(true);
    values.use_disk_shader_cache.SetGlobal(true);
    values.gpu_accuracy.SetGlobal(true);
    values.use_asynchronous_gpu_emulation.SetGlobal(true);
    values.nvdec_emulation.SetGlobal(true);
    values.accelerate_astc.SetGlobal(true);
    values.async_astc.SetGlobal(true);
    values.astc_recompression.SetGlobal(true);
    values.use_reactive_flushing.SetGlobal(true);
    values.shader_backend.SetGlobal(true);
    values.use_asynchronous_shaders.SetGlobal(true);
    values.use_fast_gpu_time.SetGlobal(true);
    values.use_vulkan_driver_pipeline_cache.SetGlobal(true);
    values.bg_red.SetGlobal(true);
    values.bg_green.SetGlobal(true);
    values.bg_blue.SetGlobal(true);
    values.enable_compute_pipelines.SetGlobal(true);

    // System
    values.language_index.SetGlobal(true);
    values.region_index.SetGlobal(true);
    values.time_zone_index.SetGlobal(true);
    values.rng_seed.SetGlobal(true);
    values.sound_index.SetGlobal(true);

    // Controls
    values.players.SetGlobal(true);
    values.use_docked_mode.SetGlobal(true);
    values.vibration_enabled.SetGlobal(true);
    values.motion_enabled.SetGlobal(true);
}

} // namespace Settings
