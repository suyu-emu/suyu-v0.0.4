// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 Torzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shared_translation.h"

#include <QCoreApplication>
#include "common/settings.h"
#include "common/settings_enums.h"
#include "common/settings_setting.h"
#include "common/time_zone.h"
#include "qt_common/config/uisettings.h"
#include <map>
#include <memory>
#include <utility>

namespace ConfigurationShared {

std::unique_ptr<TranslationMap> InitializeTranslations(QObject* parent)
{
    std::unique_ptr<TranslationMap> translations = std::make_unique<TranslationMap>();
    const auto& tr = [parent](const char* text) -> QString { return parent->tr(text); };

#define INSERT(SETTINGS, ID, NAME, TOOLTIP) \
    translations->insert(std::pair{SETTINGS::values.ID.Id(), std::pair{(NAME), (TOOLTIP)}})

    // A setting can be ignored by giving it a blank name

    // Applets
    INSERT(Settings, cabinet_applet_mode, tr("Amiibo editor"), QString());
    INSERT(Settings, controller_applet_mode, tr("Controller configuration"), QString());
    INSERT(Settings, data_erase_applet_mode, tr("Data erase"), QString());
    INSERT(Settings, error_applet_mode, tr("Error"), QString());
    INSERT(Settings, net_connect_applet_mode, tr("Net connect"), QString());
    INSERT(Settings, player_select_applet_mode, tr("Player select"), QString());
    INSERT(Settings, swkbd_applet_mode, tr("Software keyboard"), QString());
    INSERT(Settings, mii_edit_applet_mode, tr("Mii Edit"), QString());
    INSERT(Settings, web_applet_mode, tr("Online web"), QString());
    INSERT(Settings, shop_applet_mode, tr("Shop"), QString());
    INSERT(Settings, photo_viewer_applet_mode, tr("Photo viewer"), QString());
    INSERT(Settings, offline_web_applet_mode, tr("Offline web"), QString());
    INSERT(Settings, login_share_applet_mode, tr("Login share"), QString());
    INSERT(Settings, wifi_web_auth_applet_mode, tr("Wifi web auth"), QString());
    INSERT(Settings, my_page_applet_mode, tr("My page"), QString());

    // Audio
    INSERT(Settings, sink_id, tr("Output Engine:"), QString());
    INSERT(Settings, audio_output_device_id, tr("Output Device:"), QString());
    INSERT(Settings, audio_input_device_id, tr("Input Device:"), QString());
    INSERT(Settings, audio_muted, tr("Mute audio"), QString());
    INSERT(Settings, volume, tr("Volume:"), QString());
    INSERT(Settings, dump_audio_commands, QString(), QString());
    INSERT(UISettings, mute_when_in_background, tr("Mute audio when in background"), QString());

    // Core
    INSERT(
        Settings,
        use_multi_core,
        tr("Multicore CPU Emulation"),
        tr("This option increases CPU emulation thread use from 1 to the maximum of 4.\n"
           "This is mainly a debug option and shouldn't be disabled."));
    INSERT(
        Settings,
        memory_layout_mode,
        tr("Memory Layout"),
        tr("Increases the amount of emulated RAM from 4GB of the board to the "
           "devkit 8/6GB.\nDoesn't affect performance/stability but may allow HD texture "
           "mods to load."));
    INSERT(Settings, use_speed_limit, QString(), QString());
    INSERT(Settings,
           speed_limit,
           tr("Limit Speed Percent"),
           tr("Controls the game's maximum rendering speed, but it's up to each game if it runs "
              "faster or not.\n200% for a 30 FPS game is 60 FPS, and for a "
              "60 FPS game it will be 120 FPS.\nDisabling it means unlocking the framerate to the "
              "maximum your PC can reach."));
    INSERT(Settings,
           sync_core_speed,
           tr("Synchronize Core Speed"),
           tr("Synchronizes CPU core speed with the game's maximum rendering speed to boost FPS "
              "without affecting game speed (animations, physics, etc.).\n"
              "Can help reduce stuttering at lower framerates."));

    // Cpu
    INSERT(Settings,
           cpu_accuracy,
           tr("Accuracy:"),
           tr("Change the accuracy of the emulated CPU (for debugging only)."));
    INSERT(Settings, cpu_backend, tr("Backend:"), QString());

    INSERT(Settings, use_fast_cpu_time, QString(), QString());
    INSERT(Settings,
           fast_cpu_time,
           tr("Fast CPU Time"),
           tr("Overclocks the emulated CPU to remove some FPS limiters. Weaker CPUs may see reduced performance, "
              "and certain games may behave improperly.\nUse Boost (1700MHz) to run at the Switch's highest native "
              "clock, or Fast (2000MHz) to run at 2x clock."));

    INSERT(Settings, use_custom_cpu_ticks, QString(), QString());
    INSERT(Settings,
           cpu_ticks,
           tr("Custom CPU Ticks"),
           tr("Set a custom value of CPU ticks. Higher values can increase performance, but may "
              "cause deadlocks. A range of 77-21000 is recommended."));
    INSERT(Settings, cpu_backend, tr("Backend:"), QString());

    INSERT(Settings, vtable_bouncing,
        tr("Virtual Table Bouncing"),
        tr("Bounces (by emulating a 0-valued return) any functions that triggers a prefetch abort"));

    // Cpu Debug

    // Cpu Unsafe
    INSERT(Settings, cpuopt_unsafe_host_mmu, tr("Enable Host MMU Emulation (fastmem)"),
           tr("This optimization speeds up memory accesses by the guest program.\nEnabling it causes guest memory reads/writes to be done directly into memory and make use of Host's MMU.\nDisabling this forces all memory accesses to use Software MMU Emulation."));
    INSERT(
        Settings,
        cpuopt_unsafe_unfuse_fma,
        tr("Unfuse FMA (improve performance on CPUs without FMA)"),
        tr("This option improves speed by reducing accuracy of fused-multiply-add instructions on "
           "CPUs without native FMA support."));
    INSERT(
        Settings,
        cpuopt_unsafe_reduce_fp_error,
        tr("Faster FRSQRTE and FRECPE"),
        tr("This option improves the speed of some approximate floating-point functions by using "
           "less accurate native approximations."));
    INSERT(Settings,
           cpuopt_unsafe_ignore_standard_fpcr,
           tr("Faster ASIMD instructions (32 bits only)"),
           tr("This option improves the speed of 32 bits ASIMD floating-point functions by running "
              "with incorrect rounding modes."));
    INSERT(Settings,
           cpuopt_unsafe_inaccurate_nan,
           tr("Inaccurate NaN handling"),
           tr("This option improves speed by removing NaN checking.\nPlease note this also reduces "
              "accuracy of certain floating-point instructions."));
    INSERT(Settings,
           cpuopt_unsafe_fastmem_check,
           tr("Disable address space checks"),
           tr("This option improves speed by eliminating a safety check before every memory "
              "operation.\nDisabling it may allow arbitrary code execution."));
    INSERT(
        Settings,
        cpuopt_unsafe_ignore_global_monitor,
        tr("Ignore global monitor"),
        tr("This option improves speed by relying only on the semantics of cmpxchg to ensure "
           "safety of exclusive access instructions.\nPlease note this may result in deadlocks and "
           "other race conditions."));

    // Renderer
    INSERT(
        Settings,
        renderer_backend,
        tr("API:"),
        tr("Changes the output graphics API.\nVulkan is recommended."));
    INSERT(Settings,
           vulkan_device,
           tr("Device:"),
           tr("This setting selects the GPU to use (Vulkan only)."));
    INSERT(Settings,
           shader_backend,
           tr("Shader Backend:"),
           tr("The shader backend to use with OpenGL.\nGLSL is recommended."));
    INSERT(Settings,
           resolution_setup,
           tr("Resolution:"),
           tr("Forces to render at a different resolution.\n"
              "Higher resolutions require more VRAM and bandwidth.\n"
              "Options lower than 1X can cause artifacts."));
    INSERT(Settings, scaling_filter, tr("Window Adapting Filter:"), QString());
    INSERT(Settings,
           fsr_sharpening_slider,
           tr("FSR Sharpness:"),
           tr("Determines how sharpened the image will look using FSR's dynamic contrast."));
    INSERT(Settings,
           anti_aliasing,
           tr("Anti-Aliasing Method:"),
           tr("The anti-aliasing method to use.\nSMAA offers the best quality.\nFXAA "
              "can produce a more stable picture in lower resolutions."));
    INSERT(Settings,
           fullscreen_mode,
           tr("Fullscreen Mode:"),
           tr("The method used to render the window in fullscreen.\nBorderless offers the best "
              "compatibility with the on-screen keyboard that some games request for "
              "input.\nExclusive "
              "fullscreen may offer better performance and better Freesync/Gsync support."));
    INSERT(Settings,
           aspect_ratio,
           tr("Aspect Ratio:"),
           tr("Stretches the renderer to fit the specified aspect ratio.\nMost games only support "
              "16:9, so modifications are required to get other ratios.\nAlso controls the "
              "aspect ratio of captured screenshots."));
    INSERT(Settings,
           use_disk_shader_cache,
           tr("Use persistent pipeline cache"),
           tr("Allows saving shaders to storage for faster loading on following game "
              "boots.\nDisabling it is only intended for debugging."));
    INSERT(Settings,
           optimize_spirv_output,
           tr("Optimize SPIRV output"),
           tr("Runs an additional optimization pass over generated SPIRV shaders.\n"
              "Will increase time required for shader compilation.\nMay slightly improve "
              "performance.\nThis feature is experimental."));
    INSERT(
        Settings,
        use_asynchronous_gpu_emulation,
        tr("Use asynchronous GPU emulation"),
        tr("Uses an extra CPU thread for rendering.\nThis option should always remain enabled."));
    INSERT(Settings,
           nvdec_emulation,
           tr("NVDEC emulation:"),
           tr("Specifies how videos should be decoded.\nIt can either use the CPU or the GPU for "
              "decoding, or perform no decoding at all (black screen on videos).\n"
              "In most cases, GPU decoding provides the best performance."));
    INSERT(Settings,
           accelerate_astc,
           tr("ASTC Decoding Method:"),
           tr("This option controls how ASTC textures should be decoded.\n"
              "CPU: Use the CPU for decoding.\n"
              "GPU: Use the GPU's compute shaders to decode ASTC textures (recommended).\n"
              "CPU Asynchronously: Use the CPU to decode ASTC textures on demand. Eliminates"
              "ASTC decoding\nstuttering but may present artifacts."));
    INSERT(
        Settings,
        astc_recompression,
        tr("ASTC Recompression Method:"),
        tr("Most GPUs lack support for ASTC textures and must decompress to an"
            "intermediate format: RGBA8.\n"
            "BC1/BC3: The intermediate format will be recompressed to BC1 or BC3 format,\n"
            " saving VRAM but degrading image quality."));
    INSERT(Settings,
           vram_usage_mode,
           tr("VRAM Usage Mode:"),
           tr("Selects whether the emulator should prefer to conserve memory or make maximum usage of available video memory for performance.\nAggressive mode may impact performance of other applications such as recording software."));
    INSERT(Settings,
           skip_cpu_inner_invalidation,
           tr("Skip CPU Inner Invalidation"),
           tr("Skips certain cache invalidations during memory updates, reducing CPU usage and "
              "improving latency. This may cause soft-crashes."));
    INSERT(
        Settings,
        vsync_mode,
        tr("VSync Mode:"),
        tr("FIFO (VSync) does not drop frames or exhibit tearing but is limited by the screen "
           "refresh rate.\nFIFO Relaxed allows tearing as it recovers from a slow down.\n"
           "Mailbox can have lower latency than FIFO and does not tear but may drop "
           "frames.\nImmediate (no synchronization) presents whatever is available and can "
           "exhibit tearing."));
    INSERT(Settings, bg_red, QString(), QString());
    INSERT(Settings, bg_green, QString(), QString());
    INSERT(Settings, bg_blue, QString(), QString());

    // Renderer (Advanced Graphics)
    INSERT(Settings, sync_memory_operations, tr("Sync Memory Operations"),
           tr("Ensures data consistency between compute and memory operations.\nThis option fixes issues in games, but may degrade performance.\nUnreal Engine 4 games often see the most significant changes thereof."));
    INSERT(Settings,
           async_presentation,
           tr("Enable asynchronous presentation (Vulkan only)"),
           tr("Slightly improves performance by moving presentation to a separate CPU thread."));
    INSERT(
        Settings,
        renderer_force_max_clock,
        tr("Force maximum clocks (Vulkan only)"),
        tr("Runs work in the background while waiting for graphics commands to keep the GPU from "
           "lowering its clock speed."));
    INSERT(Settings,
           max_anisotropy,
           tr("Anisotropic Filtering:"),
           tr("Controls the quality of texture rendering at oblique angles.\nSafe to set at 16x on most GPUs."));
    INSERT(Settings,
           gpu_accuracy,
           tr("GPU Accuracy:"),
           tr("Controls the GPU emulation accuracy.\nMost games render fine with Normal, but High is still "
              "required for some.\nParticles tend to only render correctly with High "
              "accuracy.\nExtreme should only be used as a last resort."));
    INSERT(Settings,
           dma_accuracy,
           tr("DMA Accuracy:"),
           tr("Controls the DMA precision accuracy. Safe precision fixes issues in some games but may degrade performance."));
    INSERT(Settings,
           use_asynchronous_shaders,
           tr("Enable asynchronous shader compilation (Hack)"),
           tr("May reduce shader stutter."));
    INSERT(Settings, use_fast_gpu_time, QString(), QString());
    INSERT(Settings,
           fast_gpu_time,
           tr("Fast GPU Time (Hack)"),
           tr("Overclocks the emulated GPU to increase dynamic resolution and render "
              "distance.\nUse 128 for maximal performance and 512 for maximal graphics fidelity."));

    INSERT(Settings,
           use_vulkan_driver_pipeline_cache,
           tr("Use Vulkan pipeline cache"),
           tr("Enables GPU vendor-specific pipeline cache.\nThis option can improve shader loading "
              "time significantly in cases where the Vulkan driver does not store pipeline cache "
              "files internally."));
    INSERT(
        Settings,
        enable_compute_pipelines,
        tr("Enable Compute Pipelines (Intel Vulkan Only)"),
        tr("Required by some games.\nThis setting only exists for Intel "
           "proprietary drivers and may crash if enabled.\nCompute pipelines are always enabled "
           "on all other drivers."));
    INSERT(
        Settings,
        use_reactive_flushing,
        tr("Enable Reactive Flushing"),
        tr("Uses reactive flushing instead of predictive flushing, allowing more accurate memory "
           "syncing."));
    INSERT(Settings,
           use_video_framerate,
           tr("Sync to framerate of video playback"),
           tr("Run the game at normal speed during video playback, even when the framerate is "
              "unlocked."));
    INSERT(Settings,
           barrier_feedback_loops,
           tr("Barrier feedback loops"),
           tr("Improves rendering of transparency effects in specific games."));

    // Renderer (Extensions)
    INSERT(Settings,
           dyna_state,
           tr("Extended Dynamic State"),
           tr("Controls the number of features that can be used in Extended Dynamic State.\nHigher numbers allow for more features and can increase performance, but may cause issues.\nThe default value is per-system."));

    INSERT(Settings,
           vertex_input_dynamic_state,
           tr("Vertex Input Dynamic State"),
           tr("Enables vertex input dynamic state feature for better quality and performance."));

    INSERT(Settings,
           provoking_vertex,
           tr("Provoking Vertex"),
           tr("Improves lighting and vertex handling in some games.\n"
              "Only Vulkan 1.0+ devices support this extension."));

    INSERT(Settings,
           descriptor_indexing,
           tr("Descriptor Indexing"),
           tr("Improves texture & buffer handling and the Maxwell translation layer.\n"
              "Some Vulkan 1.1+ and all 1.2+ devices support this extension."));

    INSERT(Settings, sample_shading, QString(), QString());

    INSERT(Settings,
           sample_shading_fraction,
           tr("Sample Shading"),
           tr("Allows the fragment shader to execute per sample in a multi-sampled fragment "
              "instead of once per fragment. Improves graphics quality at the cost of performance.\n"
              "Higher values improve quality but degrade performance."));

    // Renderer (Debug)

    // System
    INSERT(Settings,
           rng_seed,
           tr("RNG Seed"),
           tr("Controls the seed of the random number generator.\nMainly used for speedrunning."));
    INSERT(Settings, rng_seed_enabled, QString(), QString());
    INSERT(Settings, device_name, tr("Device Name"), tr("The name of the console."));
    INSERT(Settings,
           custom_rtc,
           tr("Custom RTC Date:"),
           tr("This option allows to change the clock of the console.\n"
              "Can be used to manipulate time in games."));
    INSERT(Settings, custom_rtc_enabled, QString(), QString());
    INSERT(Settings,
           custom_rtc_offset,
           QStringLiteral(" "),
           tr("The number of seconds from the current unix time"));
    INSERT(Settings,
           language_index,
           tr("Language:"),
           tr("This option can be overridden when region setting is auto-select"));
    INSERT(Settings, region_index, tr("Region:"), tr("The region of the console."));
    INSERT(Settings, time_zone_index, tr("Time Zone:"), tr("The time zone of the console."));
    INSERT(Settings, sound_index, tr("Sound Output Mode:"), QString());
    INSERT(Settings,
           use_docked_mode,
           tr("Console Mode:"),
           tr("Selects if the console is in Docked or Handheld mode.\nGames will change "
              "their resolution, details and supported controllers and depending on this setting.\n"
              "Setting to Handheld can help improve performance for low end systems."));
    INSERT(Settings, current_user, QString(), QString());

    // Controls

    // Data Storage

    // Debugging

    // Debugging Graphics

    // Network

    // Web Service

    // Ui

    // Ui General
    INSERT(UISettings,
           select_user_on_boot,
           tr("Prompt for user profile on boot"),
           tr("Useful if multiple people use the same PC."));
    INSERT(UISettings,
           pause_when_in_background,
           tr("Pause when not in focus"),
           tr("Pauses emulation when focusing on other windows."));
    INSERT(UISettings,
           confirm_before_stopping,
           tr("Confirm before stopping emulation"),
           tr("Overrides prompts asking to confirm stopping the emulation.\nEnabling "
              "it bypasses such prompts and directly exits the emulation."));
    INSERT(UISettings,
           hide_mouse,
           tr("Hide mouse on inactivity"),
           tr("Hides the mouse after 2.5s of inactivity."));
    INSERT(UISettings,
           controller_applet_disabled,
           tr("Disable controller applet"),
           tr("Forcibly disables the use of the controller applet in emulated programs.\n"
                "When a program attempts to open the controller applet, it is immediately closed."));
    INSERT(UISettings,
           check_for_updates,
           tr("Check for updates"),
           tr("Whether or not to check for updates upon startup."));

    // Linux
    INSERT(Settings, enable_gamemode, tr("Enable Gamemode"), QString());
#ifdef __unix__
    INSERT(Settings, gui_force_x11, tr("Force X11 as Graphics Backend"), QString());
    INSERT(Settings, gui_hide_backend_warning, QString(), QString());
#endif

    // Ui Debugging

    // Ui Multiplayer

    // Ui Games list

#undef INSERT

    return translations;
}

std::unique_ptr<ComboboxTranslationMap> ComboboxEnumeration(QObject* parent)
{
    std::unique_ptr<ComboboxTranslationMap> translations = std::make_unique<ComboboxTranslationMap>();
    const auto& tr = [&](const char* text, const char* context = "") {
        return parent->tr(text, context);
    };

#define PAIR(ENUM, VALUE, TRANSLATION) {static_cast<u32>(Settings::ENUM::VALUE), (TRANSLATION)}

    // Intentionally skipping VSyncMode to let the UI fill that one out
    translations->insert({Settings::EnumMetadata<Settings::AppletMode>::Index(),
                          {
                              PAIR(AppletMode, HLE, tr("Custom frontend")),
                              PAIR(AppletMode, LLE, tr("Real applet")),
                          }});

    translations->insert({Settings::EnumMetadata<Settings::SpirvOptimizeMode>::Index(),
                          {
                              PAIR(SpirvOptimizeMode, Never, tr("Never")),
                              PAIR(SpirvOptimizeMode, OnLoad, tr("On Load")),
                              PAIR(SpirvOptimizeMode, Always, tr("Always")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AstcDecodeMode>::Index(),
                          {
                              PAIR(AstcDecodeMode, Cpu, tr("CPU")),
                              PAIR(AstcDecodeMode, Gpu, tr("GPU")),
                              PAIR(AstcDecodeMode, CpuAsynchronous, tr("CPU Asynchronous")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::AstcRecompression>::Index(),
         {
             PAIR(AstcRecompression, Uncompressed, tr("Uncompressed (Best quality)")),
             PAIR(AstcRecompression, Bc1, tr("BC1 (Low quality)")),
             PAIR(AstcRecompression, Bc3, tr("BC3 (Medium quality)")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::VramUsageMode>::Index(),
                          {
                              PAIR(VramUsageMode, Conservative, tr("Conservative")),
                              PAIR(VramUsageMode, Aggressive, tr("Aggressive")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::RendererBackend>::Index(),
                          {
#ifdef HAS_OPENGL
                              PAIR(RendererBackend, OpenGL, tr("OpenGL")),
#endif
                              PAIR(RendererBackend, Vulkan, tr("Vulkan")),
                              PAIR(RendererBackend, Null, tr("Null")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::ShaderBackend>::Index(),
         {
             PAIR(ShaderBackend, Glsl, tr("GLSL")),
             PAIR(ShaderBackend, Glasm, tr("GLASM (Assembly Shaders, NVIDIA Only)")),
             PAIR(ShaderBackend, SpirV, tr("SPIR-V (Experimental, AMD/Mesa Only)")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::GpuAccuracy>::Index(),
                          {
                              PAIR(GpuAccuracy, Normal, tr("Normal")),
                              PAIR(GpuAccuracy, High, tr("High")),
                              PAIR(GpuAccuracy, Extreme, tr("Extreme")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::DmaAccuracy>::Index(),
                          {
                              PAIR(DmaAccuracy, Default, tr("Default")),
                              PAIR(DmaAccuracy, Unsafe, tr("Unsafe (fast)")),
                              PAIR(DmaAccuracy, Safe, tr("Safe (stable)")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::CpuAccuracy>::Index(),
         {
             PAIR(CpuAccuracy, Auto, tr("Auto")),
             PAIR(CpuAccuracy, Accurate, tr("Accurate")),
             PAIR(CpuAccuracy, Unsafe, tr("Unsafe")),
             PAIR(CpuAccuracy, Paranoid, tr("Paranoid (disables most optimizations)")),
             PAIR(CpuAccuracy, Debugging, tr("Debugging")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::CpuBackend>::Index(),
                          {
                              PAIR(CpuBackend, Dynarmic, tr("Dynarmic")),
                              PAIR(CpuBackend, Nce, tr("NCE")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::FullscreenMode>::Index(),
                          {
                              PAIR(FullscreenMode, Borderless, tr("Borderless Windowed")),
                              PAIR(FullscreenMode, Exclusive, tr("Exclusive Fullscreen")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::NvdecEmulation>::Index(),
                          {
                              PAIR(NvdecEmulation, Off, tr("No Video Output")),
                              PAIR(NvdecEmulation, Cpu, tr("CPU Video Decoding")),
                              PAIR(NvdecEmulation, Gpu, tr("GPU Video Decoding (Default)")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::ResolutionSetup>::Index(),
         {
             PAIR(ResolutionSetup, Res1_4X, tr("0.25X (180p/270p) [EXPERIMENTAL]")),
             PAIR(ResolutionSetup, Res1_2X, tr("0.5X (360p/540p) [EXPERIMENTAL]")),
             PAIR(ResolutionSetup, Res3_4X, tr("0.75X (540p/810p) [EXPERIMENTAL]")),
             PAIR(ResolutionSetup, Res1X, tr("1X (720p/1080p)")),
             PAIR(ResolutionSetup, Res5_4X, tr("1.25X (900p/1350p) [EXPERIMENTAL]")),
             PAIR(ResolutionSetup, Res3_2X, tr("1.5X (1080p/1620p) [EXPERIMENTAL]")),
             PAIR(ResolutionSetup, Res2X, tr("2X (1440p/2160p)")),
             PAIR(ResolutionSetup, Res3X, tr("3X (2160p/3240p)")),
             PAIR(ResolutionSetup, Res4X, tr("4X (2880p/4320p)")),
             PAIR(ResolutionSetup, Res5X, tr("5X (3600p/5400p)")),
             PAIR(ResolutionSetup, Res6X, tr("6X (4320p/6480p)")),
             PAIR(ResolutionSetup, Res7X, tr("7X (5040p/7560p)")),
             PAIR(ResolutionSetup, Res8X, tr("8X (5760p/8640p)")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::ScalingFilter>::Index(),
                          {
                              PAIR(ScalingFilter, NearestNeighbor, tr("Nearest Neighbor")),
                              PAIR(ScalingFilter, Bilinear, tr("Bilinear")),
                              PAIR(ScalingFilter, Bicubic, tr("Bicubic")),
                              PAIR(ScalingFilter, Gaussian, tr("Gaussian")),
                              PAIR(ScalingFilter, Lanczos, tr("Lanczos")),
                              PAIR(ScalingFilter, ScaleForce, tr("ScaleForce")),
                              PAIR(ScalingFilter, Fsr, tr("AMD FidelityFX Super Resolution")),
                              PAIR(ScalingFilter, Area, tr("Area")),
                              PAIR(ScalingFilter, Mmpx, tr("MMPX")),
                              PAIR(ScalingFilter, ZeroTangent, tr("Zero-Tangent")),
                              PAIR(ScalingFilter, BSpline, tr("B-Spline")),
                              PAIR(ScalingFilter, Mitchell, tr("Mitchell")),
                              PAIR(ScalingFilter, Spline1, tr("Spline-1")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AntiAliasing>::Index(),
                          {
                              PAIR(AntiAliasing, None, tr("None")),
                              PAIR(AntiAliasing, Fxaa, tr("FXAA")),
                              PAIR(AntiAliasing, Smaa, tr("SMAA")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AspectRatio>::Index(),
                          {
                              PAIR(AspectRatio, R16_9, tr("Default (16:9)")),
                              PAIR(AspectRatio, R4_3, tr("Force 4:3")),
                              PAIR(AspectRatio, R21_9, tr("Force 21:9")),
                              PAIR(AspectRatio, R16_10, tr("Force 16:10")),
                              PAIR(AspectRatio, Stretch, tr("Stretch to Window")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::AnisotropyMode>::Index(),
                          {
                              PAIR(AnisotropyMode, Automatic, tr("Automatic")),
                              PAIR(AnisotropyMode, Default, tr("Default")),
                              PAIR(AnisotropyMode, X2, tr("2x")),
                              PAIR(AnisotropyMode, X4, tr("4x")),
                              PAIR(AnisotropyMode, X8, tr("8x")),
                              PAIR(AnisotropyMode, X16, tr("16x")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::Language>::Index(),
         {
             PAIR(Language, Japanese, tr("Japanese (日本語)")),
             PAIR(Language, EnglishAmerican, tr("American English")),
             PAIR(Language, French, tr("French (français)")),
             PAIR(Language, German, tr("German (Deutsch)")),
             PAIR(Language, Italian, tr("Italian (italiano)")),
             PAIR(Language, Spanish, tr("Spanish (español)")),
             PAIR(Language, Chinese, tr("Chinese")),
             PAIR(Language, Korean, tr("Korean (한국어)")),
             PAIR(Language, Dutch, tr("Dutch (Nederlands)")),
             PAIR(Language, Portuguese, tr("Portuguese (português)")),
             PAIR(Language, Russian, tr("Russian (Русский)")),
             PAIR(Language, Taiwanese, tr("Taiwanese")),
             PAIR(Language, EnglishBritish, tr("British English")),
             PAIR(Language, FrenchCanadian, tr("Canadian French")),
             PAIR(Language, SpanishLatin, tr("Latin American Spanish")),
             PAIR(Language, ChineseSimplified, tr("Simplified Chinese")),
             PAIR(Language, ChineseTraditional, tr("Traditional Chinese (正體中文)")),
             PAIR(Language, PortugueseBrazilian, tr("Brazilian Portuguese (português do Brasil)")),
             PAIR(Language, Serbian, tr("Serbian (српски)")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::Region>::Index(),
                          {
                              PAIR(Region, Japan, tr("Japan")),
                              PAIR(Region, Usa, tr("USA")),
                              PAIR(Region, Europe, tr("Europe")),
                              PAIR(Region, Australia, tr("Australia")),
                              PAIR(Region, China, tr("China")),
                              PAIR(Region, Korea, tr("Korea")),
                              PAIR(Region, Taiwan, tr("Taiwan")),
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
          PAIR(TimeZone, Cet, tr("CET")),
          PAIR(TimeZone, Cst6Cdt, tr("CST6CDT")),
          PAIR(TimeZone, Cuba, tr("Cuba")),
          PAIR(TimeZone, Eet, tr("EET")),
          PAIR(TimeZone, Egypt, tr("Egypt")),
          PAIR(TimeZone, Eire, tr("Eire")),
          PAIR(TimeZone, Est, tr("EST")),
          PAIR(TimeZone, Est5Edt, tr("EST5EDT")),
          PAIR(TimeZone, Gb, tr("GB")),
          PAIR(TimeZone, GbEire, tr("GB-Eire")),
          PAIR(TimeZone, Gmt, tr("GMT")),
          PAIR(TimeZone, GmtPlusZero, tr("GMT+0")),
          PAIR(TimeZone, GmtMinusZero, tr("GMT-0")),
          PAIR(TimeZone, GmtZero, tr("GMT0")),
          PAIR(TimeZone, Greenwich, tr("Greenwich")),
          PAIR(TimeZone, Hongkong, tr("Hongkong")),
          PAIR(TimeZone, Hst, tr("HST")),
          PAIR(TimeZone, Iceland, tr("Iceland")),
          PAIR(TimeZone, Iran, tr("Iran")),
          PAIR(TimeZone, Israel, tr("Israel")),
          PAIR(TimeZone, Jamaica, tr("Jamaica")),
          PAIR(TimeZone, Japan, tr("Japan")),
          PAIR(TimeZone, Kwajalein, tr("Kwajalein")),
          PAIR(TimeZone, Libya, tr("Libya")),
          PAIR(TimeZone, Met, tr("MET")),
          PAIR(TimeZone, Mst, tr("MST")),
          PAIR(TimeZone, Mst7Mdt, tr("MST7MDT")),
          PAIR(TimeZone, Navajo, tr("Navajo")),
          PAIR(TimeZone, Nz, tr("NZ")),
          PAIR(TimeZone, NzChat, tr("NZ-CHAT")),
          PAIR(TimeZone, Poland, tr("Poland")),
          PAIR(TimeZone, Portugal, tr("Portugal")),
          PAIR(TimeZone, Prc, tr("PRC")),
          PAIR(TimeZone, Pst8Pdt, tr("PST8PDT")),
          PAIR(TimeZone, Roc, tr("ROC")),
          PAIR(TimeZone, Rok, tr("ROK")),
          PAIR(TimeZone, Singapore, tr("Singapore")),
          PAIR(TimeZone, Turkey, tr("Turkey")),
          PAIR(TimeZone, Uct, tr("UCT")),
          PAIR(TimeZone, Universal, tr("Universal")),
          PAIR(TimeZone, Utc, tr("UTC")),
          PAIR(TimeZone, WSu, tr("W-SU")),
          PAIR(TimeZone, Wet, tr("WET")),
          PAIR(TimeZone, Zulu, tr("Zulu")),
          }});
    translations->insert({Settings::EnumMetadata<Settings::AudioMode>::Index(),
                          {
                              PAIR(AudioMode, Mono, tr("Mono")),
                              PAIR(AudioMode, Stereo, tr("Stereo")),
                              PAIR(AudioMode, Surround, tr("Surround")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::MemoryLayout>::Index(),
                          {
                              PAIR(MemoryLayout, Memory_4Gb, tr("4GB DRAM (Default)")),
                              PAIR(MemoryLayout, Memory_6Gb, tr("6GB DRAM (Unsafe)")),
                              PAIR(MemoryLayout, Memory_8Gb, tr("8GB DRAM")),
                              PAIR(MemoryLayout, Memory_10Gb, tr("10GB DRAM (Unsafe)")),
                              PAIR(MemoryLayout, Memory_12Gb, tr("12GB DRAM (Unsafe)")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::ConsoleMode>::Index(),
                          {
                              PAIR(ConsoleMode, Docked, tr("Docked")),
                              PAIR(ConsoleMode, Handheld, tr("Handheld")),
                          }});
    translations->insert({Settings::EnumMetadata<Settings::CpuClock>::Index(),
                          {
                              PAIR(CpuClock, Boost, tr("Boost (1700MHz)")),
                              PAIR(CpuClock, Fast, tr("Fast (2000MHz)")),
                          }});
    translations->insert(
        {Settings::EnumMetadata<Settings::ConfirmStop>::Index(),
         {
             PAIR(ConfirmStop, Ask_Always, tr("Always ask (Default)")),
             PAIR(ConfirmStop, Ask_Based_On_Game, tr("Only if game specifies not to stop")),
             PAIR(ConfirmStop, Ask_Never, tr("Never ask")),
         }});
    translations->insert({Settings::EnumMetadata<Settings::GpuOverclock>::Index(),
                          {
                              PAIR(GpuOverclock, Low, tr("Low (128)")),
                              PAIR(GpuOverclock, Medium, tr("Medium (256)")),
                              PAIR(GpuOverclock, High, tr("High (512)")),
                          }});

#undef PAIR
#undef CTX_PAIR

    return translations;
}
} // namespace ConfigurationShared

