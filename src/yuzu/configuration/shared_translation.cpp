// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/time_zone.h"
#include "yuzu/configuration/shared_translation.h"

#include <map>
#include <memory>
#include <tuple>
#include <utility>
#include <QWidget>
#include "common/settings.h"
#include "common/settings_enums.h"
#include "common/settings_setting.h"
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
    INSERT(Settings, audio_muted, "Mute audio", "");
    INSERT(Settings, volume, "Volume:", "");
    INSERT(Settings, dump_audio_commands, "", "");
    INSERT(UISettings, mute_when_in_background, "Mute audio when in background", "");

    // Core
    INSERT(Settings, use_multi_core, "Multicore CPU Emulation", "");
    INSERT(Settings, memory_layout_mode, "Memory Layout", "");
    INSERT(Settings, use_speed_limit, "", "");
    INSERT(Settings, speed_limit, "Limit Speed Percent", "");

    // Cpu
    INSERT(Settings, cpu_accuracy, "Accuracy:", "");

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
    INSERT(Settings, use_video_framerate, "Sync to framerate of video playback",
           "Run the game at normal speed during video playback, even when the framerate is "
           "unlocked.");
    INSERT(Settings, barrier_feedback_loops, "Barrier feedback loops",
           "Improves rendering of transparency effects in specific games.");

    // Renderer (Debug)

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
    INSERT(Settings, use_docked_mode, "Console Mode:", "");
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
    INSERT(UISettings, confirm_before_stopping, "Confirm before stopping emulation", "");
    INSERT(UISettings, hide_mouse, "Hide mouse on inactivity", "");
    INSERT(UISettings, controller_applet_disabled, "Disable controller applet", "");

    // Ui Debugging

    // Ui Multiplayer

    // Ui Games list

#undef INSERT

    return translations;
}

std::unique_ptr<ComboboxTranslationMap> ComboboxEnumeration(QWidget* parent) {
    std::unique_ptr<ComboboxTranslationMap> translations =
        std::make_unique<ComboboxTranslationMap>();
    const auto& tr = [&](const char* text, const char* context = "") {
        return parent->tr(text, context);
    };

#define PAIR(ENUM, VALUE, TRANSLATION)                                                             \
    { static_cast<u32>(Settings::ENUM::VALUE), tr(TRANSLATION) }
#define CTX_PAIR(ENUM, VALUE, TRANSLATION, CONTEXT)                                                \
    { static_cast<u32>(Settings::ENUM::VALUE), tr(TRANSLATION, CONTEXT) }

    // Intentionally skipping VSyncMode to let the UI fill that one out

    translations->insert({Settings::EnumMetadata<Settings::AstcDecodeMode>::Index(),
                          {
                              PAIR(AstcDecodeMode, Cpu, "CPU"),
                              PAIR(AstcDecodeMode, Gpu, "GPU"),
                              PAIR(AstcDecodeMode, CpuAsynchronous, "CPU Asynchronous"),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AstcRecompression>::Index(),
                          {
                              PAIR(AstcRecompression, Uncompressed, "Uncompressed (Best quality)"),
                              PAIR(AstcRecompression, Bc1, "BC1 (Low quality)"),
                              PAIR(AstcRecompression, Bc3, "BC3 (Medium quality)"),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::RendererBackend>::Index(),
                          {
#ifdef HAS_OPENGL
                              PAIR(RendererBackend, OpenGL, "OpenGL"),
#endif
                              PAIR(RendererBackend, Vulkan, "Vulkan"),
                              PAIR(RendererBackend, Null, "Null"),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::ShaderBackend>::Index(),
                          {
                              PAIR(ShaderBackend, Glsl, "GLSL"),
                              PAIR(ShaderBackend, Glasm, "GLASM (Assembly Shaders, NVIDIA Only)"),
                              PAIR(ShaderBackend, SpirV, "SPIR-V (Experimental, Mesa Only)"),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::GpuAccuracy>::Index(),
                          {
                              PAIR(GpuAccuracy, Normal, "Normal"),
                              PAIR(GpuAccuracy, High, "High"),
                              PAIR(GpuAccuracy, Extreme, "Extreme"),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::CpuAccuracy>::Index(),
                          {
                              PAIR(CpuAccuracy, Auto, "Auto"),
                              PAIR(CpuAccuracy, Accurate, "Accurate"),
                              PAIR(CpuAccuracy, Unsafe, "Unsafe"),
                              PAIR(CpuAccuracy, Paranoid, "Paranoid (disables most optimizations)"),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::FullscreenMode>::Index(),
                          {
                              PAIR(FullscreenMode, Borderless, "Borderless Windowed"),
                              PAIR(FullscreenMode, Exclusive, "Exclusive Fullscreen"),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::NvdecEmulation>::Index(),
                          {
                              PAIR(NvdecEmulation, Off, "No Video Output"),
                              PAIR(NvdecEmulation, Cpu, "CPU Video Decoding"),
                              PAIR(NvdecEmulation, Gpu, "GPU Video Decoding (Default)"),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::ResolutionSetup>::Index(),
                          {
                              PAIR(ResolutionSetup, Res1_2X, "0.5X (360p/540p) [EXPERIMENTAL]"),
                              PAIR(ResolutionSetup, Res3_4X, "0.75X (540p/810p) [EXPERIMENTAL]"),
                              PAIR(ResolutionSetup, Res1X, "1X (720p/1080p)"),
                              PAIR(ResolutionSetup, Res3_2X, "1.5X (1080p/1620p) [EXPERIMENTAL]"),
                              PAIR(ResolutionSetup, Res2X, "2X (1440p/2160p)"),
                              PAIR(ResolutionSetup, Res3X, "3X (2160p/3240p)"),
                              PAIR(ResolutionSetup, Res4X, "4X (2880p/4320p)"),
                              PAIR(ResolutionSetup, Res5X, "5X (3600p/5400p)"),
                              PAIR(ResolutionSetup, Res6X, "6X (4320p/6480p)"),
                              PAIR(ResolutionSetup, Res7X, "7X (5040p/7560p)"),
                              PAIR(ResolutionSetup, Res8X, "8X (5760p/8640p)"),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::ScalingFilter>::Index(),
                          {
                              PAIR(ScalingFilter, NearestNeighbor, "Nearest Neighbor"),
                              PAIR(ScalingFilter, Bilinear, "Bilinear"),
                              PAIR(ScalingFilter, Bicubic, "Bicubic"),
                              PAIR(ScalingFilter, Gaussian, "Gaussian"),
                              PAIR(ScalingFilter, ScaleForce, "ScaleForce"),
                              PAIR(ScalingFilter, Fsr, "AMD FidelityFX™️ Super Resolution"),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AntiAliasing>::Index(),
                          {
                              PAIR(AntiAliasing, None, "None"),
                              PAIR(AntiAliasing, Fxaa, "FXAA"),
                              PAIR(AntiAliasing, Smaa, "SMAA"),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AspectRatio>::Index(),
                          {
                              PAIR(AspectRatio, R16_9, "Default (16:9)"),
                              PAIR(AspectRatio, R4_3, "Force 4:3"),
                              PAIR(AspectRatio, R21_9, "Force 21:9"),
                              PAIR(AspectRatio, R16_10, "Force 16:10"),
                              PAIR(AspectRatio, Stretch, "Stretch to Window"),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AnisotropyMode>::Index(),
                          {
                              PAIR(AnisotropyMode, Automatic, "Automatic"),
                              PAIR(AnisotropyMode, Default, "Default"),
                              PAIR(AnisotropyMode, X2, "2x"),
                              PAIR(AnisotropyMode, X4, "4x"),
                              PAIR(AnisotropyMode, X8, "8x"),
                              PAIR(AnisotropyMode, X16, "16x"),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::Language>::Index(),
         {
             PAIR(Language, Japanese, "Japanese (日本語)"),
             PAIR(Language, EnglishAmerican, "American English"),
             PAIR(Language, French, "French (français)"),
             PAIR(Language, German, "German (Deutsch)"),
             PAIR(Language, Italian, "Italian (italiano)"),
             PAIR(Language, Spanish, "Spanish (español)"),
             PAIR(Language, Chinese, "Chinese"),
             PAIR(Language, Korean, "Korean (한국어)"),
             PAIR(Language, Dutch, "Dutch (Nederlands)"),
             PAIR(Language, Portuguese, "Portuguese (português)"),
             PAIR(Language, Russian, "Russian (Русский)"),
             PAIR(Language, Taiwanese, "Taiwanese"),
             PAIR(Language, EnglishBritish, "British English"),
             PAIR(Language, FrenchCanadian, "Canadian French"),
             PAIR(Language, SpanishLatin, "Latin American Spanish"),
             PAIR(Language, ChineseSimplified, "Simplified Chinese"),
             PAIR(Language, ChineseTraditional, "Traditional Chinese (正體中文)"),
             PAIR(Language, PortugueseBrazilian, "Brazilian Portuguese (português do Brasil)"),
         }});
    translations->insert({Settings::EnumMetadata<Settings::Region>::Index(),
                          {
                              PAIR(Region, Japan, "Japan"),
                              PAIR(Region, Usa, "USA"),
                              PAIR(Region, Europe, "Europe"),
                              PAIR(Region, Australia, "Australia"),
                              PAIR(Region, China, "China"),
                              PAIR(Region, Korea, "Korea"),
                              PAIR(Region, Taiwan, "Taiwan"),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::TimeZone>::Index(),
         {
             {static_cast<u32>(Settings::TimeZone::Auto),
              tr("Auto (%1)", "Auto select time zone")
                  .arg(QString::fromStdString(
                      Settings::GetTimeZoneString(Settings::TimeZone::Auto)))},
             {static_cast<u32>(Settings::TimeZone::Default),
              tr("Default (%1)", "Default time zone")
                  .arg(QString::fromStdString(Common::TimeZone::GetDefaultTimeZone()))},
             PAIR(TimeZone, Cet, "CET"),
             PAIR(TimeZone, Cst6Cdt, "CST6CDT"),
             PAIR(TimeZone, Cuba, "Cuba"),
             PAIR(TimeZone, Eet, "EET"),
             PAIR(TimeZone, Egypt, "Egypt"),
             PAIR(TimeZone, Eire, "Eire"),
             PAIR(TimeZone, Est, "EST"),
             PAIR(TimeZone, Est5Edt, "EST5EDT"),
             PAIR(TimeZone, Gb, "GB"),
             PAIR(TimeZone, GbEire, "GB-Eire"),
             PAIR(TimeZone, Gmt, "GMT"),
             PAIR(TimeZone, GmtPlusZero, "GMT+0"),
             PAIR(TimeZone, GmtMinusZero, "GMT-0"),
             PAIR(TimeZone, GmtZero, "GMT0"),
             PAIR(TimeZone, Greenwich, "Greenwich"),
             PAIR(TimeZone, Hongkong, "Hongkong"),
             PAIR(TimeZone, Hst, "HST"),
             PAIR(TimeZone, Iceland, "Iceland"),
             PAIR(TimeZone, Iran, "Iran"),
             PAIR(TimeZone, Israel, "Israel"),
             PAIR(TimeZone, Jamaica, "Jamaica"),
             PAIR(TimeZone, Japan, "Japan"),
             PAIR(TimeZone, Kwajalein, "Kwajalein"),
             PAIR(TimeZone, Libya, "Libya"),
             PAIR(TimeZone, Met, "MET"),
             PAIR(TimeZone, Mst, "MST"),
             PAIR(TimeZone, Mst7Mdt, "MST7MDT"),
             PAIR(TimeZone, Navajo, "Navajo"),
             PAIR(TimeZone, Nz, "NZ"),
             PAIR(TimeZone, NzChat, "NZ-CHAT"),
             PAIR(TimeZone, Poland, "Poland"),
             PAIR(TimeZone, Portugal, "Portugal"),
             PAIR(TimeZone, Prc, "PRC"),
             PAIR(TimeZone, Pst8Pdt, "PST8PDT"),
             PAIR(TimeZone, Roc, "ROC"),
             PAIR(TimeZone, Rok, "ROK"),
             PAIR(TimeZone, Singapore, "Singapore"),
             PAIR(TimeZone, Turkey, "Turkey"),
             PAIR(TimeZone, Uct, "UCT"),
             PAIR(TimeZone, Universal, "Universal"),
             PAIR(TimeZone, Utc, "UTC"),
             PAIR(TimeZone, WSu, "W-SU"),
             PAIR(TimeZone, Wet, "WET"),
             PAIR(TimeZone, Zulu, "Zulu"),
         }});
    translations->insert({Settings::EnumMetadata<Settings::AudioMode>::Index(),
                          {
                              PAIR(AudioMode, Mono, "Mono"),
                              PAIR(AudioMode, Stereo, "Stereo"),
                              PAIR(AudioMode, Surround, "Surround"),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::MemoryLayout>::Index(),
                          {
                              PAIR(MemoryLayout, Memory_4Gb, "4GB DRAM (Default)"),
                              PAIR(MemoryLayout, Memory_6Gb, "6GB DRAM (Unsafe)"),
                              PAIR(MemoryLayout, Memory_8Gb, "8GB DRAM (Unsafe)"),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::ConsoleMode>::Index(),
         {PAIR(ConsoleMode, Docked, "Docked"), PAIR(ConsoleMode, Handheld, "Handheld")}});
    translations->insert(
        {Settings::EnumMetadata<Settings::ConfirmStop>::Index(),
         {
             PAIR(ConfirmStop, Ask_Always, "Always ask (Default)"),
             PAIR(ConfirmStop, Ask_Based_On_Game, "Only if game specifies not to stop"),
             PAIR(ConfirmStop, Ask_Never, "Never ask"),
         }});

#undef PAIR
#undef CTX_PAIR

    return translations;
}
} // namespace ConfigurationShared
