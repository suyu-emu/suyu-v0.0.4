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

namespace ConfigurationShared {

std::unique_ptr<TranslationMap> InitializeTranslations(QWidget* parent) {
    std::unique_ptr<TranslationMap> translations = std::make_unique<TranslationMap>();
    const auto& tr = [parent](const char* text) -> QString { return parent->tr(text); };

#define INSERT(ID, NAME, TOOLTIP)                                                                  \
    translations->insert(std::pair{Settings::values.ID.Id(), std::pair{tr((NAME)), tr((TOOLTIP))}})

    // A setting can be ignored by giving it a blank name

    // Audio
    INSERT(sink_id, "Output Engine:", "");
    INSERT(audio_output_device_id, "Output Device:", "");
    INSERT(audio_input_device_id, "Input Device:", "");
    INSERT(audio_muted, "Mute audio when in background", "");
    INSERT(volume, "Volume:", "");

    // Core
    INSERT(use_multi_core, "Multicore CPU Emulation", "");
    INSERT(use_unsafe_extended_memory_layout, "Unsafe extended memory layout (8GB DRAM)", "");

    // Cpu
    INSERT(cpu_accuracy, "Accuracy:", "");
    INSERT(cpu_accuracy_first_time, "", "");

    // Cpu Debug

    // Cpu Unsafe
    INSERT(cpuopt_unsafe_unfuse_fma, "Unfuse FMA (improve performance on CPUs without FMA)", "");
    INSERT(cpuopt_unsafe_reduce_fp_error, "Faster FRSQRTE and FRECPE", "");
    INSERT(cpuopt_unsafe_ignore_standard_fpcr, "Faster ASIMD instructions (32 bits only)", "");
    INSERT(cpuopt_unsafe_inaccurate_nan, "Inaccurate NaN handling", "");
    INSERT(cpuopt_unsafe_fastmem_check, "Disable address space checks", "");
    INSERT(cpuopt_unsafe_ignore_global_monitor, "Ignore global monitor", "");

    // Renderer
    INSERT(renderer_backend, "API:", "");
    INSERT(vulkan_device, "Device:", "");
    INSERT(shader_backend, "Shader Backend:", "");
    INSERT(resolution_setup, "Resolution:", "");
    INSERT(scaling_filter, "Window Adapting Filter:", "");
    INSERT(fsr_sharpening_slider, "AMD FidelityFX™ Super Resolution Sharpness:", "");
    INSERT(anti_aliasing, "Anti-Aliasing Method:", "");
    INSERT(fullscreen_mode, "Fullscreen Mode:", "");
    INSERT(aspect_ratio, "Aspect Ratio:", "");
    INSERT(use_disk_shader_cache, "Use disk pipeline cache", "");
    INSERT(use_asynchronous_gpu_emulation, "Use asynchronous GPU emulation", "");
    INSERT(nvdec_emulation, "NVDEC emulation:", "");
    INSERT(accelerate_astc, "ASTC Decoding Method:", "");
    INSERT(vsync_mode, "VSync Mode:",
           "FIFO (VSync) does not drop frames or exhibit tearing but is limited by the screen "
           "refresh rate.\nFIFO Relaxed is similar to FIFO but allows tearing as it recovers from "
           "a slow down.\nMailbox can have lower latency than FIFO and does not tear but may drop "
           "frames.\nImmediate (no synchronization) just presents whatever is available and can "
           "exhibit tearing.");
    INSERT(bg_red, "", "");
    INSERT(bg_green, "", "");
    INSERT(bg_blue, "", "");

    // Renderer (Advanced Graphics)
    INSERT(async_presentation, "Enable asynchronous presentation (Vulkan only)", "");
    INSERT(renderer_force_max_clock, "Force maximum clocks (Vulkan only)",
           "Runs work in the background while waiting for graphics commands to keep the GPU from "
           "lowering its clock speed.");
    INSERT(max_anisotropy, "Anisotropic Filtering:", "");
    INSERT(gpu_accuracy, "Accuracy Level:", "");
    INSERT(use_asynchronous_shaders, "Use asynchronous shader building (Hack)",
           "Enables asynchronous shader compilation, which may reduce shader stutter. This feature "
           "is experimental.");
    INSERT(use_fast_gpu_time, "Use Fast GPU Time (Hack)",
           "Enables Fast GPU Time. This option will force most games to run at their highest "
           "native resolution.");
    INSERT(use_vulkan_driver_pipeline_cache, "Use Vulkan pipeline cache",
           "Enables GPU vendor-specific pipeline cache. This option can improve shader loading "
           "time significantly in cases where the Vulkan driver does not store pipeline cache "
           "files internally.");
    INSERT(enable_compute_pipelines, "Enable Compute Pipelines (Intel Vulkan Only)",
           "Enable compute pipelines, required by some games.\nThis setting only exists for Intel "
           "proprietary drivers, and may crash if enabled.\nCompute pipelines are always enabled "
           "on all other drivers.");
    INSERT(use_reactive_flushing, "Enable Reactive Flushing",
           "Uses reactive flushing instead of predictive flushing, allowing more accurate memory "
           "syncing.");

    // Renderer (Debug)

    // Renderer (General)
    INSERT(use_speed_limit, "Limit Speed Percent", "");
    INSERT(speed_limit, "Limit Speed Percent", "");

    // System
    INSERT(rng_seed_enabled, "RNG Seed", "");
    INSERT(rng_seed, "RNG Seed", "");
    INSERT(device_name, "Device Name", "");
    INSERT(custom_rtc_enabled, "Custom RTC", "");
    INSERT(custom_rtc, "Custom RTC", "");
    INSERT(language_index, "Language:", "");
    INSERT(region_index, "Region:", "");
    INSERT(time_zone_index, "Time Zone:", "");
    INSERT(sound_index, "Sound Output Mode:", "");
    INSERT(use_docked_mode, "", "");

    // Controls

    // Data Storage

    // Debugging

    // Debugging Graphics

    // Network

    // Web Service

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
    }

    return {};
}

} // namespace ConfigurationShared
