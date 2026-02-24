// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "common/common_types.h"
#include "common/settings_common.h"
#include "common/settings_enums.h"
#include "common/settings_input.h"
#include "common/settings_setting.h"

namespace Settings {

const char* TranslateCategory(Settings::Category category);

struct ResolutionScalingInfo {
    u32 up_scale{1};
    u32 down_shift{0};
    f32 up_factor{1.0f};
    f32 down_factor{1.0f};
    bool active{};
    bool downscale{};

    s32 ScaleUp(s32 value) const {
        if (value == 0) {
            return 0;
        }
        return (std::max)((value * static_cast<s32>(up_scale)) >> static_cast<s32>(down_shift), 1);
    }

    u32 ScaleUp(u32 value) const {
        if (value == 0U) {
            return 0U;
        }
        return (std::max)((value * up_scale) >> down_shift, 1U);
    }
};

#ifndef CANNOT_EXPLICITLY_INSTANTIATE
// Instantiate the classes elsewhere (settings.cpp) to reduce compiler/linker work
// TODO(crueter): Move new enums here
#define SETTING(TYPE, RANGED) extern template class Setting<TYPE, RANGED>
#define SWITCHABLE(TYPE, RANGED) extern template class SwitchableSetting<TYPE, RANGED>

SETTING(AudioEngine, false);
SETTING(bool, false);
SETTING(int, false);
SETTING(s32, false);
SETTING(std::string, false);
SETTING(std::string, false);
SETTING(u16, false);
SWITCHABLE(AnisotropyMode, true);
SWITCHABLE(AntiAliasing, false);
SWITCHABLE(AspectRatio, true);
SWITCHABLE(AstcDecodeMode, true);
SWITCHABLE(AstcRecompression, true);
SWITCHABLE(AudioMode, true);
SWITCHABLE(CpuBackend, true);
SWITCHABLE(CpuAccuracy, true);
SWITCHABLE(FullscreenMode, true);
SWITCHABLE(GpuAccuracy, true);
SWITCHABLE(Language, true);
SWITCHABLE(MemoryLayout, true);
SWITCHABLE(NvdecEmulation, false);
SWITCHABLE(Region, true);
SWITCHABLE(RendererBackend, true);
SWITCHABLE(ScalingFilter, false);
SWITCHABLE(SpirvOptimizeMode, true);
SWITCHABLE(TimeZone, true);
SETTING(VSyncMode, true);
SWITCHABLE(bool, false);
SWITCHABLE(int, false);
SWITCHABLE(int, true);
SWITCHABLE(s64, false);
SWITCHABLE(u16, true);
SWITCHABLE(u32, false);
SWITCHABLE(u8, false);
SWITCHABLE(u8, true);

// Used in UISettings
// TODO see if we can move this to uisettings.h
SWITCHABLE(ConfirmStop, true);

#undef SETTING
#undef SWITCHABLE
#endif

/**
 * The InputSetting class allows for getting a reference to either the global or custom members.
 * This is required as we cannot easily modify the values of user-defined types within containers
 * using the SetValue() member function found in the Setting class. The primary purpose of this
 * class is to store an array of 10 PlayerInput structs for both the global and custom setting and
 * allows for easily accessing and modifying both settings.
 */
template <typename Type>
class InputSetting final {
public:
    InputSetting() = default;
    explicit InputSetting(Type val) : Setting<Type>(val) {}
    ~InputSetting() = default;
    void SetGlobal(bool to_global) {
        use_global = to_global;
    }
    [[nodiscard]] bool UsingGlobal() const {
        return use_global;
    }
    [[nodiscard]] Type& GetValue(bool need_global = false) {
        if (use_global || need_global) {
            return global;
        }
        return custom;
    }

private:
    bool use_global{true}; ///< The setting's global state
    Type global{};         ///< The setting
    Type custom{};         ///< The custom setting value
};

struct TouchFromButtonMap {
    std::string name;
    std::vector<std::string> buttons;
};

struct Values {
    Linkage linkage{};

    // Applet
    Setting<AppletMode> cabinet_applet_mode{linkage, AppletMode::LLE, "cabinet_applet_mode",
                                            Category::LibraryApplet};
    Setting<AppletMode> controller_applet_mode{linkage, AppletMode::HLE, "controller_applet_mode",
                                               Category::LibraryApplet};
    Setting<AppletMode> data_erase_applet_mode{linkage, AppletMode::HLE, "data_erase_applet_mode",
                                               Category::LibraryApplet};
    Setting<AppletMode> error_applet_mode{linkage, AppletMode::LLE, "error_applet_mode",
                                          Category::LibraryApplet};
    Setting<AppletMode> net_connect_applet_mode{linkage, AppletMode::LLE, "net_connect_applet_mode",
                                                Category::LibraryApplet};
    Setting<AppletMode> player_select_applet_mode{
                                                  linkage, AppletMode::LLE, "player_select_applet_mode", Category::LibraryApplet};
    Setting<AppletMode> swkbd_applet_mode{linkage, AppletMode::HLE, "swkbd_applet_mode",
                                          Category::LibraryApplet};
    Setting<AppletMode> mii_edit_applet_mode{linkage, AppletMode::LLE, "mii_edit_applet_mode",
                                             Category::LibraryApplet};
    Setting<AppletMode> web_applet_mode{linkage, AppletMode::HLE, "web_applet_mode",
                                        Category::LibraryApplet};
    Setting<AppletMode> shop_applet_mode{linkage, AppletMode::HLE, "shop_applet_mode",
                                         Category::LibraryApplet};
    Setting<AppletMode> photo_viewer_applet_mode{
                                                 linkage, AppletMode::LLE, "photo_viewer_applet_mode", Category::LibraryApplet};
    Setting<AppletMode> offline_web_applet_mode{linkage, AppletMode::LLE, "offline_web_applet_mode",
                                                Category::LibraryApplet};
    Setting<AppletMode> login_share_applet_mode{linkage, AppletMode::HLE, "login_share_applet_mode",
                                                Category::LibraryApplet};
    Setting<AppletMode> wifi_web_auth_applet_mode{
                                                  linkage, AppletMode::HLE, "wifi_web_auth_applet_mode", Category::LibraryApplet};
    Setting<AppletMode> my_page_applet_mode{linkage, AppletMode::LLE, "my_page_applet_mode",
                                            Category::LibraryApplet};

    // Audio
    SwitchableSetting<AudioEngine> sink_id{linkage, AudioEngine::Auto, "output_engine",
                                           Category::Audio, Specialization::RuntimeList};
    SwitchableSetting<std::string> audio_output_device_id{
                                                          linkage, "auto", "output_device", Category::Audio, Specialization::RuntimeList};
    SwitchableSetting<std::string> audio_input_device_id{
                                                         linkage, "auto", "input_device", Category::Audio, Specialization::RuntimeList};
    SwitchableSetting<AudioMode, true> sound_index{
                                                   linkage,       AudioMode::Stereo,
                                                   "sound_index", Category::SystemAudio, Specialization::Default, true,
                                                   true};
    SwitchableSetting<u8, true> volume{linkage,
                                       100,
                                       0,
                                       200,
                                       "volume",
                                       Category::Audio,
                                       Specialization::Scalar | Specialization::Percentage,
                                       true,
                                       true};
    Setting<bool, false> audio_muted{
                                     linkage, false, "audio_muted", Category::Audio, Specialization::Default, true, true};
    Setting<bool, false> dump_audio_commands{
                                             linkage, false, "dump_audio_commands", Category::Audio, Specialization::Default, false};

    // Core
    SwitchableSetting<bool> use_multi_core{linkage, true, "use_multi_core", Category::Core};
    SwitchableSetting<MemoryLayout, true> memory_layout_mode{linkage,
                                                             MemoryLayout::Memory_4Gb,
                                                             "memory_layout_mode",
                                                             Category::Core,
                                                             Specialization::Default,
                                                             true};
    SwitchableSetting<bool> use_speed_limit{
                                            linkage, true, "use_speed_limit", Category::Core, Specialization::Paired, true, true};

    SwitchableSetting<u16, true> speed_limit{linkage,
                                             100,
                                             0,
                                             9999,
                                             "speed_limit",
                                             Category::Core,
                                             Specialization::Countable | Specialization::Percentage,
                                             true,
                                             true,
                                             &use_speed_limit};

    SwitchableSetting<u16, true> slow_speed_limit{linkage,
                                             50,
                                             0,
                                             9999,
                                             "slow_speed_limit",
                                             Category::Core,
                                             Specialization::Countable | Specialization::Percentage,
                                             true,
                                             true};

    SwitchableSetting<u16, true> turbo_speed_limit{linkage,
                                             200,
                                             0,
                                             9999,
                                             "turbo_speed_limit",
                                             Category::Core,
                                             Specialization::Countable | Specialization::Percentage,
                                             true,
                                             true};

    // The currently used speed mode.
    Setting<SpeedMode> current_speed_mode{linkage, SpeedMode::Standard, "current_speed_mode", Category::Core, Specialization::Default, false, true};

    SwitchableSetting<bool> sync_core_speed{linkage, false, "sync_core_speed", Category::Core,
                                            Specialization::Default};

    // Cpu
    SwitchableSetting<CpuBackend, true> cpu_backend{linkage,
#ifdef HAS_NCE
                                                    CpuBackend::Nce,
#else
                                                    CpuBackend::Dynarmic,
#endif
                                                    CpuBackend::Dynarmic,
#ifdef HAS_NCE
                                                    CpuBackend::Nce,
#else
                                                    CpuBackend::Dynarmic,
#endif
                                                    "cpu_backend",
                                                    Category::Cpu};
    SwitchableSetting<CpuAccuracy, true> cpu_accuracy{linkage, CpuAccuracy::Auto,
                                                      "cpu_accuracy", Category::Cpu};
    SwitchableSetting<CpuClock> fast_cpu_time{linkage,
                                              CpuClock::Off,
                                              "fast_cpu_time",
                                              Category::Cpu,
                                              Specialization::Default,
                                              true,
                                              true};

    SwitchableSetting<bool> use_custom_cpu_ticks{linkage,
                                                 false,
                                                 "use_custom_cpu_ticks",
                                                 Category::Cpu,
                                                 Specialization::Paired,
                                                 true,
                                                 true};

    SwitchableSetting<u32, true> cpu_ticks{linkage,
                                           16000,
                                           77,
                                           65535,
                                           "cpu_ticks",
                                           Category::Cpu,
                                           Specialization::Countable,
                                           true,
                                           true,
                                           &use_custom_cpu_ticks};

    SwitchableSetting<bool> vtable_bouncing{linkage, true, "vtable_bouncing", Category::Cpu};

    Setting<bool> cpuopt_page_tables{linkage, true, "cpuopt_page_tables", Category::CpuDebug};
    Setting<bool> cpuopt_block_linking{linkage, true, "cpuopt_block_linking", Category::CpuDebug};
    Setting<bool> cpuopt_return_stack_buffer{linkage, true, "cpuopt_return_stack_buffer",
                                             Category::CpuDebug};
    Setting<bool> cpuopt_fast_dispatcher{linkage, true, "cpuopt_fast_dispatcher",
                                         Category::CpuDebug};
    Setting<bool> cpuopt_context_elimination{linkage, true, "cpuopt_context_elimination",
                                             Category::CpuDebug};
    Setting<bool> cpuopt_const_prop{linkage, true, "cpuopt_const_prop", Category::CpuDebug};
    Setting<bool> cpuopt_misc_ir{linkage, true, "cpuopt_misc_ir", Category::CpuDebug};
    Setting<bool> cpuopt_reduce_misalign_checks{linkage, true, "cpuopt_reduce_misalign_checks",
                                                Category::CpuDebug};
    SwitchableSetting<bool> cpuopt_fastmem{linkage, true, "cpuopt_fastmem", Category::CpuDebug};
    SwitchableSetting<bool> cpuopt_fastmem_exclusives{linkage, true, "cpuopt_fastmem_exclusives",
                                                      Category::CpuDebug};
    Setting<bool> cpuopt_recompile_exclusives{linkage, true, "cpuopt_recompile_exclusives",
                                              Category::CpuDebug};
    Setting<bool> cpuopt_ignore_memory_aborts{linkage, true, "cpuopt_ignore_memory_aborts",
                                              Category::CpuDebug};

    SwitchableSetting<bool> cpuopt_unsafe_host_mmu{linkage,
#if !defined(__APPLE__) && !defined(__linux__) && !defined(__ANDROID__) && !defined(_WIN32)
                                        false,
#else
                                        true,
#endif
                                        "cpuopt_unsafe_host_mmu",
                                        Category::CpuUnsafe};
    SwitchableSetting<bool> cpuopt_unsafe_unfuse_fma{linkage, true, "cpuopt_unsafe_unfuse_fma",
                                                     Category::CpuUnsafe};
    SwitchableSetting<bool> cpuopt_unsafe_reduce_fp_error{
                                                          linkage, true, "cpuopt_unsafe_reduce_fp_error", Category::CpuUnsafe};
    SwitchableSetting<bool> cpuopt_unsafe_ignore_standard_fpcr{
                                                               linkage, true, "cpuopt_unsafe_ignore_standard_fpcr", Category::CpuUnsafe};
    SwitchableSetting<bool> cpuopt_unsafe_inaccurate_nan{
                                                         linkage, true, "cpuopt_unsafe_inaccurate_nan", Category::CpuUnsafe};
    SwitchableSetting<bool> cpuopt_unsafe_fastmem_check{
                                                        linkage, true, "cpuopt_unsafe_fastmem_check", Category::CpuUnsafe};
    SwitchableSetting<bool> cpuopt_unsafe_ignore_global_monitor{
                                                                linkage, true, "cpuopt_unsafe_ignore_global_monitor", Category::CpuUnsafe};

    // Renderer
    SwitchableSetting<RendererBackend, true> renderer_backend{linkage,
#if defined(__sun__) || defined(__managarm__)
        RendererBackend::OpenGL_GLSL,
#else
        RendererBackend::Vulkan,
#endif
        "backend", Category::Renderer};
    SwitchableSetting<int> vulkan_device{linkage, 0, "vulkan_device", Category::Renderer, Specialization::RuntimeList};

    // Graphics Settings
    ResolutionScalingInfo resolution_info{};
    SwitchableSetting<ResolutionSetup> resolution_setup{linkage, ResolutionSetup::Res1X,
                                                        "resolution_setup", Category::Renderer};

    SwitchableSetting<VSyncMode, true> vsync_mode{linkage,
                                                  VSyncMode::Fifo,
                                                  "use_vsync",
                                                  Category::Renderer,
                                                  Specialization::RuntimeList,
                                                  true,
                                                  true};

    SwitchableSetting<ScalingFilter> scaling_filter{linkage,
                                                    ScalingFilter::Bilinear,
                                                    "scaling_filter",
                                                    Category::Renderer,
                                                    Specialization::Default,
                                                    true,
                                                    true};
    SwitchableSetting<int, true> fsr_sharpening_slider{linkage,
                                                       25,
                                                       0,
                                                       200,
                                                       "fsr_sharpening_slider",
                                                       Category::Renderer,
                                                       Specialization::Scalar |
                                                           Specialization::Percentage,
                                                       true,
                                                       true};
    SwitchableSetting<AspectRatio, true> aspect_ratio{linkage,
                                                      AspectRatio::R16_9,
                                                      "aspect_ratio",
                                                      Category::Renderer,
                                                      Specialization::Default,
                                                      true,
                                                      true};

    SwitchableSetting<AntiAliasing> anti_aliasing{linkage,
                                                  AntiAliasing::None,
                                                  "anti_aliasing",
                                                  Category::Renderer,
                                                  Specialization::Default,
                                                  true,
                                                  true};

    SwitchableSetting<SpirvOptimizeMode, true> optimize_spirv_output{linkage,
                                                                     SpirvOptimizeMode::Never,
                                                                     "optimize_spirv_output",
                                                                     Category::Renderer};
    SwitchableSetting<bool> use_asynchronous_gpu_emulation{
                                                           linkage, true, "use_asynchronous_gpu_emulation", Category::Renderer};
    // *nix platforms may have issues with the borderless windowed fullscreen mode.
    // Default to exclusive fullscreen on these platforms for now.
    SwitchableSetting<FullscreenMode, true> fullscreen_mode{linkage,
#ifdef _WIN32
                                                            FullscreenMode::Borderless,
#else
                                                            FullscreenMode::Exclusive,
#endif
                                                            "fullscreen_mode",
                                                            Category::Renderer,
                                                            Specialization::Default,
                                                            true,
                                                            true};

    SwitchableSetting<u8, false> bg_red{
                                        linkage, 0, "bg_red", Category::Renderer, Specialization::Default, true, true};
    SwitchableSetting<u8, false> bg_green{
                                          linkage, 0, "bg_green", Category::Renderer, Specialization::Default, true, true};
    SwitchableSetting<u8, false> bg_blue{
                                         linkage, 0, "bg_blue", Category::Renderer, Specialization::Default, true, true};

    SwitchableSetting<GpuAccuracy, true> gpu_accuracy{linkage,
#ifdef ANDROID
                                                      GpuAccuracy::Low,
#else
                                                      GpuAccuracy::Medium,
#endif
                                                      "gpu_accuracy",
                                                      Category::RendererAdvanced,
                                                      Specialization::Default,
                                                      true,
                                                      true};

    GpuAccuracy current_gpu_accuracy{GpuAccuracy::Medium};

    SwitchableSetting<DmaAccuracy, true> dma_accuracy{linkage,
                                                      DmaAccuracy::Default,
                                                      "dma_accuracy",
                                                      Category::RendererAdvanced,
                                                      Specialization::Default,
                                                      true,
                                                      true};

    SwitchableSetting<VramUsageMode, true> vram_usage_mode{linkage,
                                                           VramUsageMode::Conservative,
                                                           "vram_usage_mode",
                                                           Category::RendererAdvanced};

    SwitchableSetting<NvdecEmulation> nvdec_emulation{linkage, NvdecEmulation::Gpu,
                                                      "nvdec_emulation", Category::RendererAdvanced};

    SwitchableSetting<AnisotropyMode, true> max_anisotropy{linkage,
#ifdef ANDROID
                                                           AnisotropyMode::Default,
#else
                                                           AnisotropyMode::Automatic,
#endif
                                                           "max_anisotropy",
                                                           Category::RendererAdvanced};
    SwitchableSetting<AstcDecodeMode, true> accelerate_astc{linkage,
#ifdef ANDROID
                                                            AstcDecodeMode::Cpu,
#else
                                                            AstcDecodeMode::Gpu,
#endif
                                                            "accelerate_astc",
                                                            Category::RendererAdvanced};

    SwitchableSetting<FramePacingMode, true> frame_pacing_mode{linkage,
                                                               FramePacingMode::Target_Auto,
                                                               FramePacingMode::Target_Auto,
                                                               FramePacingMode::Target_120,
                                                               "frame_pacing_mode",
                                                               Category::RendererAdvanced,
                                                               Specialization::Default,
                                                               true,
                                                               true};

    SwitchableSetting<AstcRecompression, true> astc_recompression{linkage,
                                                                  AstcRecompression::Uncompressed,
                                                                  "astc_recompression",
                                                                  Category::RendererAdvanced};


    SwitchableSetting<bool> sync_memory_operations{linkage,
                                                   false,
                                                   "sync_memory_operations",
                                                   Category::RendererAdvanced,
                                                   Specialization::Default,
                                                   true,
                                                   true};

    SwitchableSetting<bool> renderer_force_max_clock{linkage, false, "force_max_clock",
                                                     Category::RendererAdvanced};

    SwitchableSetting<bool> use_disk_shader_cache{linkage, true, "use_disk_shader_cache",
                                                  Category::RendererAdvanced};

    SwitchableSetting<bool> use_vulkan_driver_pipeline_cache{
        linkage, true, "use_vulkan_driver_pipeline_cache", Category::RendererAdvanced,
        Specialization::Default};

    SwitchableSetting<bool> enable_compute_pipelines{linkage, false, "enable_compute_pipelines",
                                                     Category::RendererAdvanced};

    SwitchableSetting<bool> use_video_framerate{linkage, false, "use_video_framerate",
                                                Category::RendererAdvanced};

    SwitchableSetting<bool> use_reactive_flushing{linkage,
#ifdef ANDROID
                                                  false,
#else
                                                  true,
#endif
                                                  "use_reactive_flushing",
                                                  Category::RendererAdvanced};

    SwitchableSetting<bool> barrier_feedback_loops{linkage, true, "barrier_feedback_loops",
                                                   Category::RendererAdvanced};

    SwitchableSetting<bool> enable_buffer_history{linkage,
                                                  false,
                                                  "enable_buffer_history",
                                                  Category::RendererAdvanced,
                                                  Specialization::Default,
                                                  true,
                                                  true};

    // Renderer Hacks //
    SwitchableSetting<GpuOverclock> fast_gpu_time{linkage,
                                                  GpuOverclock::Medium,
                                                  "fast_gpu_time",
                                                  Category::RendererHacks,
                                                  Specialization::Default,
                                                        true,
                                                        true};

    SwitchableSetting<bool> skip_cpu_inner_invalidation{linkage,
                                                        false,
                                                        "skip_cpu_inner_invalidation",
                                                        Category::RendererHacks,
                                                        Specialization::Default,
                                                        true,
                                                        true};
    SwitchableSetting<bool> async_presentation{linkage,
#ifdef ANDROID
                                               true,
#else
                                               false,
#endif
                                               "async_presentation", Category::RendererHacks};

    SwitchableSetting<bool> fix_bloom_effects{linkage, false, "fix_bloom_effects",
                                                     Category::RendererHacks};

    SwitchableSetting<bool> use_asynchronous_shaders{linkage, false, "use_asynchronous_shaders",
                                                     Category::RendererHacks};

    SwitchableSetting<GpuUnswizzleSize> gpu_unswizzle_texture_size{linkage,
                                                  GpuUnswizzleSize::Large,
                                                  "gpu_unswizzle_texture_size",
                                                  Category::RendererHacks,
                                                  Specialization::Default};

    SwitchableSetting<GpuUnswizzle> gpu_unswizzle_stream_size{linkage,
                                                  GpuUnswizzle::Medium,
                                                  "gpu_unswizzle_stream_size",
                                                  Category::RendererHacks,
                                                  Specialization::Default};

    SwitchableSetting<GpuUnswizzleChunk> gpu_unswizzle_chunk_size{linkage,
                                                  GpuUnswizzleChunk::Medium,
                                                  "gpu_unswizzle_chunk_size",
                                                  Category::RendererHacks,
                                                  Specialization::Default};

    SwitchableSetting<bool> gpu_unswizzle_enabled{linkage, false, "gpu_unswizzle_enabled",
                                                  Category::RendererHacks};

    SwitchableSetting<ExtendedDynamicState> dyna_state{linkage,
#if defined (ANDROID) || defined (__APPLE__)
                                           ExtendedDynamicState::Disabled,
#else
                                           ExtendedDynamicState::EDS2,
#endif
                                           "dyna_state",
                                           Category::RendererExtensions};

    SwitchableSetting<u32, true> sample_shading{linkage,
                                                0,
                                                0,
                                                100,
                                                "sample_shading_fraction",
                                                Category::RendererExtensions,
                                                Specialization::Scalar};

    SwitchableSetting<bool> vertex_input_dynamic_state{linkage,
#if defined (ANDROID)
                                                       false,
#else
                                                       true,
#endif
                                                       "vertex_input_dynamic_state", Category::RendererExtensions};
    SwitchableSetting<bool> provoking_vertex{linkage, false, "provoking_vertex", Category::RendererExtensions};
    SwitchableSetting<bool> descriptor_indexing{linkage, false, "descriptor_indexing", Category::RendererExtensions};

    Setting<bool> renderer_debug{linkage, false, "debug", Category::RendererDebug};
    Setting<bool> renderer_shader_feedback{linkage, false, "shader_feedback",
                                           Category::RendererDebug};
    Setting<bool> enable_nsight_aftermath{linkage, false, "nsight_aftermath",
                                          Category::RendererDebug};
    Setting<bool> disable_shader_loop_safety_checks{
                                                    linkage, false, "disable_shader_loop_safety_checks", Category::RendererDebug};
    Setting<bool> enable_renderdoc_hotkey{linkage, false, "renderdoc_hotkey",
                                          Category::RendererDebug};
#if defined(ANDROID) && defined(ARCHITECTURE_arm64)
    // Debug override for automatic BCn patching detection
    Setting<bool> patch_old_qcom_drivers{linkage, false, "patch_old_qcom_drivers",
                                         Category::RendererDebug};
#endif
    SwitchableSetting<bool> disable_buffer_reorder{linkage, false, "disable_buffer_reorder",
                                         Category::RendererDebug,
                                         Specialization::Default,
                                                         true,
                                                         true};

    // System
    SwitchableSetting<Language, true> language_index{linkage,
                                                     Language::EnglishAmerican,
                                                     "language_index",
                                                     Category::System};
    SwitchableSetting<Region, true> region_index{linkage, Region::Usa, "region_index", Category::System};
    SwitchableSetting<TimeZone, true> time_zone_index{linkage, TimeZone::Auto, "time_zone_index", Category::System};
    Setting<u32> serial_battery{linkage, 0, "serial_battery", Category::System};
    Setting<u32> serial_unit{linkage, 0, "serial_unit", Category::System};
    // Measured in seconds since epoch
    SwitchableSetting<bool> custom_rtc_enabled{linkage, false, "custom_rtc_enabled", Category::System, Specialization::Paired, true, true};
    SwitchableSetting<s64> custom_rtc{
                                      linkage, 0,    "custom_rtc",       Category::System, Specialization::Time,
                                      false,   true, &custom_rtc_enabled};
    SwitchableSetting<s64, true> custom_rtc_offset{linkage,
                                                   0,
                                                   (std::numeric_limits<int>::min)(),
                                                   (std::numeric_limits<int>::max)(),
                                                   "custom_rtc_offset",
                                                   Category::System,
                                                   Specialization::Countable,
                                                   true,
                                                   true};
    SwitchableSetting<bool> rng_seed_enabled{
                                             linkage, false, "rng_seed_enabled", Category::System, Specialization::Paired, true, true};
    SwitchableSetting<u32> rng_seed{
                                    linkage, 0,    "rng_seed",       Category::System, Specialization::Hex,
                                    true,    true, &rng_seed_enabled};
    Setting<std::string> device_name{
        linkage, "Eden", "device_name", Category::System, Specialization::Default, true, true};

    Setting<s32> current_user{linkage, 0, "current_user", Category::System};

    SwitchableSetting<ConsoleMode> use_docked_mode{linkage,
#ifdef ANDROID
                                                   ConsoleMode::Handheld,
#else
                                                   ConsoleMode::Docked,
#endif
                                                   "use_docked_mode",
                                                   Category::System,
                                                   Specialization::Radio,
                                                   true,
                                                   true};

    // Controls
    InputSetting<std::array<PlayerInput, 10>> players;

    Setting<bool> enable_raw_input{
        linkage, false, "enable_raw_input", Category::Controls, Specialization::Default,
// Only read/write enable_raw_input on Windows platforms
#ifdef _WIN32
        true
#else
        false
#endif
    };
    Setting<bool> controller_navigation{linkage, true, "controller_navigation", Category::Controls};
    Setting<bool> enable_joycon_driver{linkage, true, "enable_joycon_driver", Category::Controls};
    Setting<bool> enable_procon_driver{linkage, false, "enable_procon_driver", Category::Controls};

    SwitchableSetting<bool> vibration_enabled{linkage, true, "vibration_enabled",
                                              Category::Controls};
    SwitchableSetting<bool> enable_accurate_vibrations{linkage, false, "enable_accurate_vibrations",
                                                       Category::Controls};

    SwitchableSetting<bool> motion_enabled{linkage, true, "motion_enabled", Category::Controls};
    Setting<std::string> udp_input_servers{linkage, "127.0.0.1:26760", "udp_input_servers",
                                           Category::Controls};
    Setting<bool> enable_udp_controller{linkage, false, "enable_udp_controller",
                                        Category::Controls};

    Setting<bool> pause_tas_on_load{linkage, true, "pause_tas_on_load", Category::Controls};
    Setting<bool> tas_enable{linkage, false, "tas_enable", Category::Controls};
    Setting<bool> tas_loop{linkage, false, "tas_loop", Category::Controls};

    Setting<bool> mouse_panning{
                                linkage, false, "mouse_panning", Category::Controls, Specialization::Default, false};
    Setting<u8, true> mouse_panning_sensitivity{
                                                linkage, 50, 1, 100, "mouse_panning_sensitivity", Category::Controls};
    Setting<bool> mouse_enabled{linkage, false, "mouse_enabled", Category::Controls};

    Setting<u8, true> mouse_panning_x_sensitivity{
                                                  linkage, 50, 1, 100, "mouse_panning_x_sensitivity", Category::Controls};
    Setting<u8, true> mouse_panning_y_sensitivity{
                                                  linkage, 50, 1, 100, "mouse_panning_y_sensitivity", Category::Controls};
    Setting<u8, true> mouse_panning_deadzone_counterweight{
                                                           linkage, 20, 0, 100, "mouse_panning_deadzone_counterweight", Category::Controls};
    Setting<u8, true> mouse_panning_decay_strength{
                                                   linkage, 18, 0, 100, "mouse_panning_decay_strength", Category::Controls};
    Setting<u8, true> mouse_panning_min_decay{
                                              linkage, 6, 0, 100, "mouse_panning_min_decay", Category::Controls};

    Setting<bool> emulate_analog_keyboard{linkage, false, "emulate_analog_keyboard",
                                          Category::Controls};
    Setting<bool> keyboard_enabled{linkage, false, "keyboard_enabled", Category::Controls};

    Setting<bool> debug_pad_enabled{linkage, false, "debug_pad_enabled", Category::Controls};
    ButtonsRaw debug_pad_buttons;
    AnalogsRaw debug_pad_analogs;

    TouchscreenInput touchscreen;

    Setting<std::string> touch_device{linkage, "min_x:100,min_y:50,max_x:1800,max_y:850",
                                      "touch_device", Category::Controls};
    Setting<int> touch_from_button_map_index{linkage, 0, "touch_from_button_map",
                                             Category::Controls};
    std::vector<TouchFromButtonMap> touch_from_button_maps;

    Setting<bool> enable_ring_controller{linkage, true, "enable_ring_controller",
                                         Category::Controls};
    RingconRaw ringcon_analogs;

    Setting<bool> enable_ir_sensor{linkage, false, "enable_ir_sensor", Category::Controls};
    Setting<std::string> ir_sensor_device{linkage, "auto", "ir_sensor_device", Category::Controls};

    Setting<bool> random_amiibo_id{linkage, false, "random_amiibo_id", Category::Controls};

    // Data Storage
    Setting<bool> use_virtual_sd{linkage, true, "use_virtual_sd", Category::DataStorage};
    Setting<bool> gamecard_inserted{linkage, false, "gamecard_inserted", Category::DataStorage};
    Setting<bool> gamecard_current_game{linkage, false, "gamecard_current_game",
                                        Category::DataStorage};
    Setting<std::string> gamecard_path{linkage, std::string(), "gamecard_path",
                                       Category::DataStorage};
    std::vector<std::string> external_content_dirs;

    // Debugging
    bool record_frame_times;
    Setting<bool> use_gdbstub{linkage, false, "use_gdbstub", Category::Debugging};
    Setting<u16> gdbstub_port{linkage, 6543, "gdbstub_port", Category::Debugging};
    Setting<std::string> program_args{linkage, std::string(), "program_args", Category::Debugging};
    Setting<bool> dump_exefs{linkage, false, "dump_exefs", Category::Debugging};
    Setting<bool> dump_nso{linkage, false, "dump_nso", Category::Debugging};
    Setting<bool> dump_shaders{
                               linkage, false, "dump_shaders", Category::DebuggingGraphics, Specialization::Default,
                               false};
    Setting<bool> dump_macros{
                              linkage, false, "dump_macros", Category::DebuggingGraphics, Specialization::Default, false};
    Setting<bool> enable_fs_access_log{linkage, false, "enable_fs_access_log", Category::Debugging};
    Setting<bool> reporting_services{
                                     linkage, false, "reporting_services", Category::Debugging, Specialization::Default, false};
    Setting<bool> quest_flag{linkage, false, "quest_flag", Category::Debugging};
    Setting<bool> disable_macro_jit{linkage, false, "disable_macro_jit",
                                    Category::DebuggingGraphics};
    Setting<bool> disable_macro_hle{linkage, false, "disable_macro_hle",
                                    Category::DebuggingGraphics};
    Setting<bool> extended_logging{
                                   linkage, false, "extended_logging", Category::Debugging, Specialization::Default, false};
    Setting<bool> use_debug_asserts{linkage, false, "use_debug_asserts", Category::Debugging};
    Setting<bool> use_auto_stub{
                                linkage, false, "use_auto_stub", Category::Debugging};
    Setting<bool> enable_all_controllers{linkage, false, "enable_all_controllers",
                                         Category::Debugging};
    Setting<bool> perform_vulkan_check{linkage, true, "perform_vulkan_check", Category::Debugging};
    Setting<bool> disable_web_applet{linkage, true, "disable_web_applet", Category::Debugging};

    // GPU Logging
    Setting<bool> gpu_logging_enabled{linkage, false, "gpu_logging_enabled", Category::Debugging};
    SwitchableSetting<GpuLogLevel> gpu_log_level{linkage, GpuLogLevel::Standard, "gpu_log_level",
                                                   Category::Debugging};
    Setting<bool> gpu_log_vulkan_calls{linkage, true, "gpu_log_vulkan_calls", Category::Debugging};
    Setting<bool> gpu_log_shader_dumps{linkage, false, "gpu_log_shader_dumps", Category::Debugging};
    Setting<bool> gpu_log_memory_tracking{linkage, true, "gpu_log_memory_tracking",
                                           Category::Debugging};
    Setting<bool> gpu_log_driver_debug{linkage, true, "gpu_log_driver_debug", Category::Debugging};
    Setting<s32> gpu_log_ring_buffer_size{linkage, 512, "gpu_log_ring_buffer_size",
                                           Category::Debugging};

    SwitchableSetting<u16, true> debug_knobs{linkage,
                                           0,
                                           0,
                                           65535,
                                           "debug_knobs",
                                           Category::Core,
                                           Specialization::Countable,
                                           true,
                                           true};

    // Miscellaneous
    Setting<std::string> log_filter{linkage, "*:Info", "log_filter", Category::Miscellaneous};
    Setting<bool> log_flush_line{linkage, false, "flush_line", Category::Miscellaneous, Specialization::Default, true, true};
    Setting<bool> censor_username{linkage, true, "censor_username", Category::Miscellaneous};
    Setting<bool> first_launch{linkage, true, "first_launch", Category::Miscellaneous};

    // Network
    Setting<std::string> network_interface{linkage, std::string(), "network_interface",
                                           Category::Network};
    SwitchableSetting<bool> airplane_mode{linkage, false, "airplane_mode", Category::Network};

    // WebService
    Setting<std::string> web_api_url{linkage, "api.ynet-fun.xyz", "web_api_url",
                                     Category::WebService};
    Setting<std::string> eden_username{linkage, "Eden", "eden_username",
                                       Category::WebService};
    Setting<std::string> eden_token{linkage, "",
                                    "eden_token", Category::WebService};

    // Add-Ons
    std::map<u64, std::vector<std::string>> disabled_addons;

    // Per-game overrides
    bool use_squashed_iterated_blend;

    Setting<bool> enable_overlay{linkage, false, "enable_overlay", Category::Core};
};

extern Values values;

bool getDebugKnobAt(u8 i);

void UpdateGPUAccuracy();
bool IsGPULevelLow();
bool IsGPULevelMedium();
bool IsGPULevelHigh();

bool IsDMALevelDefault();
bool IsDMALevelSafe();

bool IsFastmemEnabled();
void SetNceEnabled(bool is_64bit);
bool IsNceEnabled();

bool IsDockedMode();

float Volume();

// speed limit ops
u16 SpeedLimit();
void SetSpeedMode(const SpeedMode &mode);
void ToggleStandardMode();
void ToggleTurboMode();
void ToggleSlowMode();

std::string GetTimeZoneString(TimeZone time_zone);

void LogSettings();

void TranslateResolutionInfo(ResolutionSetup setup, ResolutionScalingInfo& info);
void UpdateRescalingInfo();

// Restore the global state of all applicable settings in the Values struct
void RestoreGlobalState(bool is_powered_on);

bool IsConfiguringGlobal();
void SetConfiguringGlobal(bool is_global);

} // namespace Settings
