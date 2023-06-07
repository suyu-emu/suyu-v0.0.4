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
    INSERT(Settings, dump_audio_commands, "", "");

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
           "Unfuse FMA (improve performance on CPUs without FMA)",
           "This option improves speed by reducing accuracy of fused-multiply-add instructions on "
           "CPUs without native FMA support.");
    INSERT(Settings, cpuopt_unsafe_reduce_fp_error, "Faster FRSQRTE and FRECPE",
           "This option improves the speed of some approximate floating-point functions by using "
           "less accurate native approximations.");
    INSERT(Settings, cpuopt_unsafe_ignore_standard_fpcr, "Faster ASIMD instructions (32 bits only)",
           "This option improves the speed of 32 bits ASIMD floating-point functions by running "
           "with incorrect rounding modes.");
    INSERT(Settings, cpuopt_unsafe_inaccurate_nan, "Inaccurate NaN handling",
           "This option improves speed by removing NaN checking. Please note this also reduces "
           "accuracy of certain floating-point instructions.");
    INSERT(
        Settings, cpuopt_unsafe_fastmem_check, "Disable address space checks",
        "This option improves speed by eliminating a safety check before every memory read/write "
        "in guest. Disabling it may allow a game to read/write the emulator's memory.");
    INSERT(Settings, cpuopt_unsafe_ignore_global_monitor, "Ignore global monitor",
           "This option improves speed by relying only on the semantics of cmpxchg to ensure "
           "safety of exclusive access instructions. Please note this may result in deadlocks and "
           "other race conditions.");

    // Renderer
    INSERT(Settings, renderer_backend, "API:", "");
    INSERT(Settings, vulkan_device, "Device:", "");
    INSERT(Settings, shader_backend, "Shader Backend:", "");
    INSERT(Settings, resolution_setup, "Resolution:", "");
    INSERT(Settings, scaling_filter, "Window Adapting Filter:", "");
    INSERT(Settings, fsr_sharpening_slider, "FSR Sharpness:", "");
    INSERT(Settings, anti_aliasing, "Anti-Aliasing Method:", "");
    INSERT(Settings, fullscreen_mode, "Fullscreen Mode:", "");
    INSERT(Settings, aspect_ratio, "Aspect Ratio:", "");
    INSERT(Settings, use_disk_shader_cache, "Use disk pipeline cache", "");
    INSERT(Settings, use_asynchronous_gpu_emulation, "Use asynchronous GPU emulation", "");
    INSERT(Settings, nvdec_emulation, "NVDEC emulation:", "");
    INSERT(Settings, accelerate_astc, "ASTC Decoding Method:", "");
    INSERT(Settings, astc_recompression, "ASTC Recompression Method:", "");
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
    INSERT(Settings, rng_seed, "RNG Seed", "");
    INSERT(Settings, rng_seed_enabled, "", "");
    INSERT(Settings, device_name, "Device Name", "");
    INSERT(Settings, custom_rtc, "Custom RTC", "");
    INSERT(Settings, custom_rtc_enabled, "", "");
    INSERT(Settings, language_index,
           "Language:", "Note: this can be overridden when region setting is auto-select");
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

std::unique_ptr<ComboboxTranslationMap> ComboboxEnumeration(QWidget* parent) {
    std::unique_ptr<ComboboxTranslationMap> translations =
        std::make_unique<ComboboxTranslationMap>();
    const auto& tr = [&](const char* text) { return parent->tr(text); };

    // Intentionally skipping VSyncMode to let the UI fill that one out

    translations->insert(
        {typeid(Settings::AstcDecodeMode),
         {
             {static_cast<u32>(Settings::AstcDecodeMode::Cpu), tr("CPU")},
             {static_cast<u32>(Settings::AstcDecodeMode::Gpu), tr("GPU")},
             {static_cast<u32>(Settings::AstcDecodeMode::CpuAsynchronous), tr("CPU Asynchronous")},
         }});
    translations->insert(
        {typeid(Settings::AstcRecompression),
         {
             {static_cast<u32>(Settings::AstcRecompression::Uncompressed),
              tr("Uncompressed (Best quality)")},
             {static_cast<u32>(Settings::AstcRecompression::Bc1), tr("BC1 (Low quality)")},
             {static_cast<u32>(Settings::AstcRecompression::Bc3), tr("BC3 (Medium quality)")},
         }});
    translations->insert({typeid(Settings::RendererBackend),
                          {
#ifdef HAS_OPENGL
                              {static_cast<u32>(Settings::RendererBackend::OpenGL), tr("OpenGL")},
#endif
                              {static_cast<u32>(Settings::RendererBackend::Vulkan), tr("Vulkan")},
                              {static_cast<u32>(Settings::RendererBackend::Null), tr("Null")},
                          }});
    translations->insert({typeid(Settings::ShaderBackend),
                          {
                              {static_cast<u32>(Settings::ShaderBackend::Glsl), tr("GLSL")},
                              {static_cast<u32>(Settings::ShaderBackend::Glasm),
                               tr("GLASM (Assembly Shaders, NVIDIA Only)")},
                              {static_cast<u32>(Settings::ShaderBackend::SpirV),
                               tr("SPIR-V (Experimental, Mesa Only)")},
                          }});
    translations->insert({typeid(Settings::GpuAccuracy),
                          {
                              {static_cast<u32>(Settings::GpuAccuracy::Normal), tr("Normal")},
                              {static_cast<u32>(Settings::GpuAccuracy::High), tr("High")},
                              {static_cast<u32>(Settings::GpuAccuracy::Extreme), tr("Extreme")},
                          }});
    translations->insert({typeid(Settings::CpuAccuracy),
                          {
                              {static_cast<u32>(Settings::CpuAccuracy::Auto), tr("Auto")},
                              {static_cast<u32>(Settings::CpuAccuracy::Accurate), tr("Accurate")},
                              {static_cast<u32>(Settings::CpuAccuracy::Unsafe), tr("Unsafe")},
                              {static_cast<u32>(Settings::CpuAccuracy::Paranoid),
                               tr("Paranoid (disables most optimizations)")},
                          }});
    translations->insert(
        {typeid(Settings::FullscreenMode),
         {
             {static_cast<u32>(Settings::FullscreenMode::Borderless), tr("Borderless Windowed")},
             {static_cast<u32>(Settings::FullscreenMode::Exclusive), tr("Exclusive Fullscreen")},
         }});
    translations->insert(
        {typeid(Settings::NvdecEmulation),
         {
             {static_cast<u32>(Settings::NvdecEmulation::Off), tr("No Video Output")},
             {static_cast<u32>(Settings::NvdecEmulation::Cpu), tr("CPU Video Decoding")},
             {static_cast<u32>(Settings::NvdecEmulation::Gpu), tr("GPU Video Decoding (Default)")},
         }});
    translations->insert(
        {typeid(Settings::ResolutionSetup),
         {
             {static_cast<u32>(Settings::ResolutionSetup::Res1_2X),
              tr("0.5X (360p/540p) [EXPERIMENTAL]")},
             {static_cast<u32>(Settings::ResolutionSetup::Res3_4X),
              tr("0.75X (540p/810p) [EXPERIMENTAL]")},
             {static_cast<u32>(Settings::ResolutionSetup::Res1X), tr("1X (720p/1080p)")},
             {static_cast<u32>(Settings::ResolutionSetup::Res3_2X),
              tr("1.5X (1080p/1620p) [EXPERIMENTAL]")},
             {static_cast<u32>(Settings::ResolutionSetup::Res2X), tr("2X (1440p/2160p)")},
             {static_cast<u32>(Settings::ResolutionSetup::Res3X), tr("3X (2160p/3240p)")},
             {static_cast<u32>(Settings::ResolutionSetup::Res4X), tr("4X (2880p/4320p)")},
             {static_cast<u32>(Settings::ResolutionSetup::Res5X), tr("5X (3600p/5400p)")},
             {static_cast<u32>(Settings::ResolutionSetup::Res6X), tr("6X (4320p/6480p)")},
             {static_cast<u32>(Settings::ResolutionSetup::Res7X), tr("7X (5040p/7560p)")},
             {static_cast<u32>(Settings::ResolutionSetup::Res8X), tr("8X (5760p/8640p)")},
         }});
    translations->insert(
        {typeid(Settings::ScalingFilter),
         {
             {static_cast<u32>(Settings::ScalingFilter::NearestNeighbor), tr("Nearest Neighbor")},
             {static_cast<u32>(Settings::ScalingFilter::Bilinear), tr("Bilinear")},
             {static_cast<u32>(Settings::ScalingFilter::Bicubic), tr("Bicubic")},
             {static_cast<u32>(Settings::ScalingFilter::Gaussian), tr("Gaussian")},
             {static_cast<u32>(Settings::ScalingFilter::ScaleForce), tr("ScaleForce")},
             {static_cast<u32>(Settings::ScalingFilter::Fsr),
              tr("AMD FidelityFX™️ Super Resolution")},
         }});
    translations->insert({typeid(Settings::AntiAliasing),
                          {
                              {static_cast<u32>(Settings::AntiAliasing::None), tr("None")},
                              {static_cast<u32>(Settings::AntiAliasing::Fxaa), tr("FXAA")},
                              {static_cast<u32>(Settings::AntiAliasing::Smaa), tr("SMAA")},
                          }});
    translations->insert(
        {typeid(Settings::AspectRatio),
         {
             {static_cast<u32>(Settings::AspectRatio::R16_9), tr("Default (16:9)")},
             {static_cast<u32>(Settings::AspectRatio::R4_3), tr("Force 4:3")},
             {static_cast<u32>(Settings::AspectRatio::R21_9), tr("Force 21:9")},
             {static_cast<u32>(Settings::AspectRatio::R16_10), tr("Force 16:10")},
             {static_cast<u32>(Settings::AspectRatio::Stretch), tr("Stretch to Window")},
         }});
    translations->insert(
        {typeid(Settings::AnisotropyMode),
         {
             {static_cast<u32>(Settings::AnisotropyMode::Automatic), tr("Automatic")},
             {static_cast<u32>(Settings::AnisotropyMode::Default), tr("Default")},
             {static_cast<u32>(Settings::AnisotropyMode::X2), tr("2x")},
             {static_cast<u32>(Settings::AnisotropyMode::X4), tr("4x")},
             {static_cast<u32>(Settings::AnisotropyMode::X8), tr("8x")},
             {static_cast<u32>(Settings::AnisotropyMode::X16), tr("16x")},
         }});
    translations->insert(
        {typeid(Settings::Language),
         {
             {static_cast<u32>(Settings::Language::Japanese), tr("Japanese (日本語)")},
             {static_cast<u32>(Settings::Language::EnglishAmerican), tr("American English")},
             {static_cast<u32>(Settings::Language::French), tr("French (français)")},
             {static_cast<u32>(Settings::Language::German), tr("German (Deutsch)")},
             {static_cast<u32>(Settings::Language::Italian), tr("Italian (italiano)")},
             {static_cast<u32>(Settings::Language::Spanish), tr("Spanish (español)")},
             {static_cast<u32>(Settings::Language::Chinese), tr("Chinese")},
             {static_cast<u32>(Settings::Language::Korean), tr("Korean (한국어)")},
             {static_cast<u32>(Settings::Language::Dutch), tr("Dutch (Nederlands)")},
             {static_cast<u32>(Settings::Language::Portuguese), tr("Portuguese (português)")},
             {static_cast<u32>(Settings::Language::Russian), tr("Russian (Русский)")},
             {static_cast<u32>(Settings::Language::Taiwanese), tr("Taiwanese")},
             {static_cast<u32>(Settings::Language::EnglishBritish), tr("British English")},
             {static_cast<u32>(Settings::Language::FrenchCanadian), tr("Canadian French")},
             {static_cast<u32>(Settings::Language::SpanishLatin), tr("Latin American Spanish")},
             {static_cast<u32>(Settings::Language::ChineseSimplified), tr("Simplified Chinese")},
             {static_cast<u32>(Settings::Language::ChineseTraditional),
              tr("Traditional Chinese (正體中文)")},
             {static_cast<u32>(Settings::Language::PortugueseBrazilian),
              tr("Brazilian Portuguese (português do Brasil)")},
         }});
    translations->insert({typeid(Settings::Region),
                          {
                              {static_cast<u32>(Settings::Region::Japan), tr("Japan")},
                              {static_cast<u32>(Settings::Region::Usa), tr("USA")},
                              {static_cast<u32>(Settings::Region::Europe), tr("Europe")},
                              {static_cast<u32>(Settings::Region::Australia), tr("Australia")},
                              {static_cast<u32>(Settings::Region::China), tr("China")},
                              {static_cast<u32>(Settings::Region::Korea), tr("Korea")},
                              {static_cast<u32>(Settings::Region::Taiwan), tr("Taiwan")},
                          }});
    translations->insert({typeid(Settings::TimeZone),
                          {
                              {static_cast<u32>(Settings::TimeZone::Auto), tr("Auto")},
                              {static_cast<u32>(Settings::TimeZone::Default), tr("Default")},
                              {static_cast<u32>(Settings::TimeZone::CET), tr("CET")},
                              {static_cast<u32>(Settings::TimeZone::CST6CDT), tr("CST6CDT")},
                              {static_cast<u32>(Settings::TimeZone::Cuba), tr("Cuba")},
                              {static_cast<u32>(Settings::TimeZone::EET), tr("EET")},
                              {static_cast<u32>(Settings::TimeZone::Egypt), tr("Egypt")},
                              {static_cast<u32>(Settings::TimeZone::Eire), tr("Eire")},
                              {static_cast<u32>(Settings::TimeZone::EST), tr("EST")},
                              {static_cast<u32>(Settings::TimeZone::EST5EDT), tr("EST5EDT")},
                              {static_cast<u32>(Settings::TimeZone::GB), tr("GB")},
                              {static_cast<u32>(Settings::TimeZone::GBEire), tr("GB-Eire")},
                              {static_cast<u32>(Settings::TimeZone::GMT), tr("GMT")},
                              {static_cast<u32>(Settings::TimeZone::GMTPlusZero), tr("GMT+0")},
                              {static_cast<u32>(Settings::TimeZone::GMTMinusZero), tr("GMT-0")},
                              {static_cast<u32>(Settings::TimeZone::GMTZero), tr("GMT0")},
                              {static_cast<u32>(Settings::TimeZone::Greenwich), tr("Greenwich")},
                              {static_cast<u32>(Settings::TimeZone::Hongkong), tr("Hongkong")},
                              {static_cast<u32>(Settings::TimeZone::HST), tr("HST")},
                              {static_cast<u32>(Settings::TimeZone::Iceland), tr("Iceland")},
                              {static_cast<u32>(Settings::TimeZone::Iran), tr("Iran")},
                              {static_cast<u32>(Settings::TimeZone::Israel), tr("Israel")},
                              {static_cast<u32>(Settings::TimeZone::Jamaica), tr("Jamaica")},
                              {static_cast<u32>(Settings::TimeZone::Kwajalein), tr("Kwajalein")},
                              {static_cast<u32>(Settings::TimeZone::Libya), tr("Libya")},
                              {static_cast<u32>(Settings::TimeZone::MET), tr("MET")},
                              {static_cast<u32>(Settings::TimeZone::MST), tr("MST")},
                              {static_cast<u32>(Settings::TimeZone::MST7MDT), tr("MST7MDT")},
                              {static_cast<u32>(Settings::TimeZone::Navajo), tr("Navajo")},
                              {static_cast<u32>(Settings::TimeZone::NZ), tr("NZ")},
                              {static_cast<u32>(Settings::TimeZone::NZCHAT), tr("NZ-CHAT")},
                              {static_cast<u32>(Settings::TimeZone::Poland), tr("Poland")},
                              {static_cast<u32>(Settings::TimeZone::Portugal), tr("Portugal")},
                              {static_cast<u32>(Settings::TimeZone::PRC), tr("PRC")},
                              {static_cast<u32>(Settings::TimeZone::PST8PDT), tr("PST8PDT")},
                              {static_cast<u32>(Settings::TimeZone::ROC), tr("ROC")},
                              {static_cast<u32>(Settings::TimeZone::ROK), tr("ROK")},
                              {static_cast<u32>(Settings::TimeZone::Singapore), tr("Singapore")},
                              {static_cast<u32>(Settings::TimeZone::Turkey), tr("Turkey")},
                              {static_cast<u32>(Settings::TimeZone::UCT), tr("UCT")},
                              {static_cast<u32>(Settings::TimeZone::W_SU), tr("W-SU")},
                              {static_cast<u32>(Settings::TimeZone::WET), tr("WET")},
                              {static_cast<u32>(Settings::TimeZone::Zulu), tr("Zulu")},
                          }});
    translations->insert({typeid(Settings::AudioMode),
                          {
                              {static_cast<u32>(Settings::AudioMode::Mono), tr("Mono")},
                              {static_cast<u32>(Settings::AudioMode::Stereo), tr("Stereo")},
                              {static_cast<u32>(Settings::AudioMode::Surround), tr("Surround")},
                          }});

    return translations;
}
} // namespace ConfigurationShared
