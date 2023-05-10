// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <forward_list>
#include <map>
#include <memory>
#include <string>
#include <typeindex>
#include <utility>
#include <QString>
#include <QWidget>
#include "common/settings.h"
#include "yuzu/configuration/shared_translation.h"
#include "yuzu/uisettings.h"

namespace ConfigurationShared {

std::unique_ptr<TranslationMap> InitializeTranslations(QWidget* parent) {
    std::unique_ptr<TranslationMap> translations = std::make_unique<TranslationMap>();
    const auto& tr = [parent](const char* text) -> QString { return parent->tr(text); };

#define INSERT(SETTINGS, ID, NAME, TOOLTIP)                                                        \
    translations->insert(std::pair{SETTINGS::values.ID.Id(), std::pair{tr((NAME)), tr((TOOLTIP))}})

    // A setting can be ignored by giving it a blank name

    // Audio
    INSERT(Settings, sink_id, "Output Engine:", "");
    INSERT(Settings, audio_output_device_id, "Output Device:", "");
    INSERT(Settings, audio_input_device_id, "Input Device:", "");
    INSERT(Settings, audio_muted, "Mute audio when in background", "");
    INSERT(Settings, volume, "Volume:", "");

    // Core
    INSERT(Settings, use_multi_core, "Multicore CPU Emulation", "");
    INSERT(Settings, use_unsafe_extended_memory_layout, "Unsafe extended memory layout (8GB DRAM)",
           "");

    // Cpu
    INSERT(Settings, cpu_accuracy, "Accuracy:", "");
    INSERT(Settings, cpu_accuracy_first_time, "", "");

    // Cpu Debug

    // Cpu Unsafe
    INSERT(Settings, cpuopt_unsafe_unfuse_fma,
           "Unfuse FMA (improve performance on CPUs without FMA)", "");
    INSERT(Settings, cpuopt_unsafe_reduce_fp_error, "Faster FRSQRTE and FRECPE", "");
    INSERT(Settings, cpuopt_unsafe_ignore_standard_fpcr, "Faster ASIMD instructions (32 bits only)",
           "");
    INSERT(Settings, cpuopt_unsafe_inaccurate_nan, "Inaccurate NaN handling", "");
    INSERT(Settings, cpuopt_unsafe_fastmem_check, "Disable address space checks", "");
    INSERT(Settings, cpuopt_unsafe_ignore_global_monitor, "Ignore global monitor", "");

    // Renderer
    INSERT(Settings, renderer_backend, "API:", "");
    INSERT(Settings, vulkan_device, "Device:", "");
    INSERT(Settings, shader_backend, "Shader Backend:", "");
    INSERT(Settings, resolution_setup, "Resolution:", "");
    INSERT(Settings, scaling_filter, "Window Adapting Filter:", "");
    INSERT(Settings, fsr_sharpening_slider, "AMD FidelityFX™ Super Resolution Sharpness:", "");
    INSERT(Settings, anti_aliasing, "Anti-Aliasing Method:", "");
    INSERT(Settings, fullscreen_mode, "Fullscreen Mode:", "");
    INSERT(Settings, aspect_ratio, "Aspect Ratio:", "");
    INSERT(Settings, use_disk_shader_cache, "Use disk pipeline cache", "");
    INSERT(Settings, use_asynchronous_gpu_emulation, "Use asynchronous GPU emulation", "");
    INSERT(Settings, nvdec_emulation, "NVDEC emulation:", "");
    INSERT(Settings, accelerate_astc, "ASTC Decoding Method:", "");
    INSERT(Settings, vsync_mode, "VSync Mode:",
           "FIFO (VSync) does not drop frames or exhibit tearing but is limited by the screen "
           "refresh rate.\nFIFO Relaxed is similar to FIFO but allows tearing as it recovers from "
           "a slow down.\nMailbox can have lower latency than FIFO and does not tear but may drop "
           "frames.\nImmediate (no synchronization) just presents whatever is available and can "
           "exhibit tearing.");
    INSERT(Settings, bg_red, "", "");
    INSERT(Settings, bg_green, "", "");
    INSERT(Settings, bg_blue, "", "");

    // Renderer (Advanced Graphics)
    INSERT(Settings, async_presentation, "Enable asynchronous presentation (Vulkan only)", "");
    INSERT(Settings, renderer_force_max_clock, "Force maximum clocks (Vulkan only)",
           "Runs work in the background while waiting for graphics commands to keep the GPU from "
           "lowering its clock speed.");
    INSERT(Settings, max_anisotropy, "Anisotropic Filtering:", "");
    INSERT(Settings, gpu_accuracy, "Accuracy Level:", "");
    INSERT(Settings, use_asynchronous_shaders, "Use asynchronous shader building (Hack)",
           "Enables asynchronous shader compilation, which may reduce shader stutter. This feature "
           "is experimental.");
    INSERT(Settings, use_fast_gpu_time, "Use Fast GPU Time (Hack)",
           "Enables Fast GPU Time. This option will force most games to run at their highest "
           "native resolution.");
    INSERT(Settings, use_vulkan_driver_pipeline_cache, "Use Vulkan pipeline cache",
           "Enables GPU vendor-specific pipeline cache. This option can improve shader loading "
           "time significantly in cases where the Vulkan driver does not store pipeline cache "
           "files internally.");
    INSERT(Settings, enable_compute_pipelines, "Enable Compute Pipelines (Intel Vulkan Only)",
           "Enable compute pipelines, required by some games.\nThis setting only exists for Intel "
           "proprietary drivers, and may crash if enabled.\nCompute pipelines are always enabled "
           "on all other drivers.");
    INSERT(Settings, use_reactive_flushing, "Enable Reactive Flushing",
           "Uses reactive flushing instead of predictive flushing, allowing more accurate memory "
           "syncing.");

    // Renderer (Debug)

    // Renderer (General)
    INSERT(Settings, use_speed_limit, "", "");
    INSERT(Settings, speed_limit, "Limit Speed Percent", "");

    // System
    INSERT(Settings, rng_seed_enabled, "RNG Seed", "");
    INSERT(Settings, rng_seed, "", "");
    INSERT(Settings, device_name, "Device Name", "");
    INSERT(Settings, custom_rtc_enabled, "Custom RTC", "");
    INSERT(Settings, custom_rtc, "", "");
    INSERT(Settings, language_index, "Language:", "");
    INSERT(Settings, region_index, "Region:", "");
    INSERT(Settings, time_zone_index, "Time Zone:", "");
    INSERT(Settings, sound_index, "Sound Output Mode:", "");
    INSERT(Settings, use_docked_mode, "", "");
    INSERT(Settings, current_user, "", "");

    // Controls

    // Data Storage

    // Debugging

    // Debugging Graphics

    // Network

    // Web Service

    // Ui

    // Ui General
    INSERT(UISettings, select_user_on_boot, "Prompt for user on game boot", "");
    INSERT(UISettings, pause_when_in_background, "Pause emulation when in background", "");
    INSERT(UISettings, confirm_before_closing, "Confirm exit while emulation is running", "");
    INSERT(UISettings, hide_mouse, "Hide mouse on inactivity", "");

    // Ui Debugging

    // Ui Multiplayer

    // Ui Games list

#undef INSERT

    return translations;
}

std::forward_list<QString> ComboboxEnumeration(std::type_index type, QWidget* parent) {
    const auto& tr = [&](const char* text) { return parent->tr(text); };

    // Intentionally skipping VSyncMode to let the UI fill that one out

    if (type == typeid(Settings::AstcDecodeMode)) {
        return {
            tr("CPU"),
            tr("GPU"),
            tr("CPU Asynchronous"),
        };
    } else if (type == typeid(Settings::RendererBackend)) {
        return {
            tr("OpenGL"),
            tr("Vulkan"),
            tr("Null"),
        };
    } else if (type == typeid(Settings::ShaderBackend)) {
        return {
            tr("GLSL"),
            tr("GLASM (Assembly Shaders, NVIDIA Only)"),
            tr("SPIR-V (Experimental, Mesa Only)"),
        };
    } else if (type == typeid(Settings::GPUAccuracy)) {
        return {
            tr("Normal"),
            tr("High"),
            tr("Extreme"),
        };
    } else if (type == typeid(Settings::CPUAccuracy)) {
        return {
            tr("Auto"),
            tr("Accurate"),
            tr("Unsafe"),
            tr("Paranoid (disables most optimizations)"),
        };
    } else if (type == typeid(Settings::FullscreenMode)) {
        return {
            tr("Borderless Windowed"),
            tr("Exclusive Fullscreen"),
        };
    } else if (type == typeid(Settings::NvdecEmulation)) {
        return {
            tr("No Video Output"),
            tr("CPU Video Decoding"),
            tr("GPU Video Decoding (Default)"),
        };
    } else if (type == typeid(Settings::ResolutionSetup)) {
        return {
            tr("0.5X (360p/540p) [EXPERIMENTAL]"),
            tr("0.75X (540p/810p) [EXPERIMENTAL]"),
            tr("1X (720p/1080p)"),
            tr("1.5X (1080p/1620p) [EXPERIMENTAL]"),
            tr("2X (1440p/2160p)"),
            tr("3X (2160p/3240p)"),
            tr("4X (2880p/4320p)"),
            tr("5X (3600p/5400p)"),
            tr("6X (4320p/6480p)"),
            tr("7X (5040p/7560p)"),
            tr("8X (5760p/8640p)"),
        };
    } else if (type == typeid(Settings::ScalingFilter)) {
        return {
            tr("Nearest Neighbor"), tr("Bilinear"),   tr("Bicubic"),
            tr("Gaussian"),         tr("ScaleForce"), tr("AMD FidelityFX™️ Super Resolution"),
        };
    } else if (type == typeid(Settings::AntiAliasing)) {
        return {
            tr("None"),
            tr("FXAA"),
            tr("SMAA"),
        };
    } else if (type == typeid(Settings::AspectRatio)) {
        return {
            tr("Default (16:9)"), tr("Force 4:3"),         tr("Force 21:9"),
            tr("Force 16:10"),    tr("Stretch to Window"),
        };
    } else if (type == typeid(Settings::AnisotropyMode)) {
        return {
            tr("Automatic"), tr("Default"), tr("2x"), tr("4x"), tr("8x"), tr("16x"),
        };
    } else if (type == typeid(Settings::Language)) {
        return {
            tr("Japanese (日本語)"),
            tr("American English"),
            tr("French (français)"),
            tr("German (Deutsch)"),
            tr("Italian (italiano)"),
            tr("Spanish (español)"),
            tr("Chinese"),
            tr("Korean (한국어)"),
            tr("Dutch (Nederlands)"),
            tr("Portuguese (português)"),
            tr("Russian (Русский)"),
            tr("Taiwanese"),
            tr("British English"),
            tr("Canadian French"),
            tr("Latin American Spanish"),
            tr("Simplified Chinese"),
            tr("Traditional Chinese (正體中文)"),
            tr("Brazilian Portuguese (português do Brasil)"),
        };
    } else if (type == typeid(Settings::Region)) {
        return {
            tr("Japan"), tr("USA"),   tr("Europe"), tr("Australia"),
            tr("China"), tr("Korea"), tr("Taiwan"),
        };
    } else if (type == typeid(Settings::TimeZone)) {
        return {
            tr("Auto"),    tr("Default"),   tr("CET"),       tr("CST6CDT"),   tr("Cuba"),
            tr("EET"),     tr("Egypt"),     tr("Eire"),      tr("EST"),       tr("EST5EDT"),
            tr("GB"),      tr("GB-Eire"),   tr("GMT"),       tr("GMT+0"),     tr("GMT-0"),
            tr("GMT0"),    tr("Greenwich"), tr("Hongkong"),  tr("HST"),       tr("Iceland"),
            tr("Iran"),    tr("Israel"),    tr("Jamaica"),   tr("Kwajalein"), tr("Libya"),
            tr("MET"),     tr("MST"),       tr("MST7MDT"),   tr("Navajo"),    tr("NZ"),
            tr("NZ-CHAT"), tr("Poland"),    tr("Portugal"),  tr("PRC"),       tr("PST8PDT"),
            tr("ROC"),     tr("ROK"),       tr("Singapore"), tr("Turkey"),    tr("UCT"),
            tr("W-SU"),    tr("WET"),       tr("Zulu"),
        };
    }

    return {};
}

} // namespace ConfigurationShared
