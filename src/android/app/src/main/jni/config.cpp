// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <optional>
#include <sstream>

#include <INIReader.h>
#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/settings_enums.h"
#include "core/hle/service/acc/profile_manager.h"
#include "input_common/main.h"
#include "jni/config.h"
#include "jni/default_ini.h"
#include "uisettings.h"

namespace FS = Common::FS;

Config::Config(const std::string& config_name, ConfigType config_type)
    : type(config_type), global{config_type == ConfigType::GlobalConfig} {
    Initialize(config_name);
}

Config::~Config() = default;

bool Config::LoadINI(const std::string& default_contents, bool retry) {
    void(FS::CreateParentDir(config_loc));
    config = std::make_unique<INIReader>(FS::PathToUTF8String(config_loc));
    const auto config_loc_str = FS::PathToUTF8String(config_loc);
    if (config->ParseError() < 0) {
        if (retry) {
            LOG_WARNING(Config, "Failed to load {}. Creating file from defaults...",
                        config_loc_str);

            void(FS::CreateParentDir(config_loc));
            void(FS::WriteStringToFile(config_loc, FS::FileType::TextFile, default_contents));

            config = std::make_unique<INIReader>(config_loc_str);

            return LoadINI(default_contents, false);
        }
        LOG_ERROR(Config, "Failed.");
        return false;
    }
    LOG_INFO(Config, "Successfully loaded {}", config_loc_str);
    return true;
}

template <>
void Config::ReadSetting(const std::string& group, Settings::Setting<std::string>& setting) {
    std::string setting_value = config->Get(group, setting.GetLabel(), setting.GetDefault());
    if (setting_value.empty()) {
        setting_value = setting.GetDefault();
    }
    setting = std::move(setting_value);
}

template <>
void Config::ReadSetting(const std::string& group, Settings::Setting<bool>& setting) {
    setting = config->GetBoolean(group, setting.GetLabel(), setting.GetDefault());
}

template <typename Type, bool ranged>
void Config::ReadSetting(const std::string& group, Settings::Setting<Type, ranged>& setting) {
    setting = static_cast<Type>(
        config->GetInteger(group, setting.GetLabel(), static_cast<long>(setting.GetDefault())));
}

void Config::ReadValues() {
    ReadSetting("ControlsGeneral", Settings::values.mouse_enabled);
    ReadSetting("ControlsGeneral", Settings::values.touch_device);
    ReadSetting("ControlsGeneral", Settings::values.keyboard_enabled);
    ReadSetting("ControlsGeneral", Settings::values.debug_pad_enabled);
    ReadSetting("ControlsGeneral", Settings::values.vibration_enabled);
    ReadSetting("ControlsGeneral", Settings::values.enable_accurate_vibrations);
    ReadSetting("ControlsGeneral", Settings::values.motion_enabled);
    Settings::values.touchscreen.enabled =
        config->GetBoolean("ControlsGeneral", "touch_enabled", true);
    Settings::values.touchscreen.rotation_angle =
        config->GetInteger("ControlsGeneral", "touch_angle", 0);
    Settings::values.touchscreen.diameter_x =
        config->GetInteger("ControlsGeneral", "touch_diameter_x", 15);
    Settings::values.touchscreen.diameter_y =
        config->GetInteger("ControlsGeneral", "touch_diameter_y", 15);

    int num_touch_from_button_maps =
        config->GetInteger("ControlsGeneral", "touch_from_button_map", 0);
    if (num_touch_from_button_maps > 0) {
        for (int i = 0; i < num_touch_from_button_maps; ++i) {
            Settings::TouchFromButtonMap map;
            map.name = config->Get("ControlsGeneral",
                                   std::string("touch_from_button_maps_") + std::to_string(i) +
                                       std::string("_name"),
                                   "default");
            const int num_touch_maps = config->GetInteger(
                "ControlsGeneral",
                std::string("touch_from_button_maps_") + std::to_string(i) + std::string("_count"),
                0);
            map.buttons.reserve(num_touch_maps);

            for (int j = 0; j < num_touch_maps; ++j) {
                std::string touch_mapping =
                    config->Get("ControlsGeneral",
                                std::string("touch_from_button_maps_") + std::to_string(i) +
                                    std::string("_bind_") + std::to_string(j),
                                "");
                map.buttons.emplace_back(std::move(touch_mapping));
            }

            Settings::values.touch_from_button_maps.emplace_back(std::move(map));
        }
    } else {
        Settings::values.touch_from_button_maps.emplace_back(
            Settings::TouchFromButtonMap{"default", {}});
        num_touch_from_button_maps = 1;
    }
    Settings::values.touch_from_button_map_index = std::clamp(
        Settings::values.touch_from_button_map_index.GetValue(), 0, num_touch_from_button_maps - 1);

    ReadSetting("ControlsGeneral", Settings::values.udp_input_servers);

    // Data Storage
    ReadSetting("Data Storage", Settings::values.use_virtual_sd);
    FS::SetYuzuPath(FS::YuzuPath::NANDDir,
                    config->Get("Data Storage", "nand_directory",
                                FS::GetYuzuPathString(FS::YuzuPath::NANDDir)));
    FS::SetYuzuPath(FS::YuzuPath::SDMCDir,
                    config->Get("Data Storage", "sdmc_directory",
                                FS::GetYuzuPathString(FS::YuzuPath::SDMCDir)));
    FS::SetYuzuPath(FS::YuzuPath::LoadDir,
                    config->Get("Data Storage", "load_directory",
                                FS::GetYuzuPathString(FS::YuzuPath::LoadDir)));
    FS::SetYuzuPath(FS::YuzuPath::DumpDir,
                    config->Get("Data Storage", "dump_directory",
                                FS::GetYuzuPathString(FS::YuzuPath::DumpDir)));
    ReadSetting("Data Storage", Settings::values.gamecard_inserted);
    ReadSetting("Data Storage", Settings::values.gamecard_current_game);
    ReadSetting("Data Storage", Settings::values.gamecard_path);

    // System
    ReadSetting("System", Settings::values.current_user);
    Settings::values.current_user = std::clamp<int>(Settings::values.current_user.GetValue(), 0,
                                                    Service::Account::MAX_USERS - 1);

    // Disable docked mode by default on Android
    Settings::values.use_docked_mode.SetValue(config->GetBoolean("System", "use_docked_mode", false)
                                                  ? Settings::ConsoleMode::Docked
                                                  : Settings::ConsoleMode::Handheld);

    const auto rng_seed_enabled = config->GetBoolean("System", "rng_seed_enabled", false);
    if (rng_seed_enabled) {
        Settings::values.rng_seed.SetValue(config->GetInteger("System", "rng_seed", 0));
    } else {
        Settings::values.rng_seed.SetValue(0);
    }
    Settings::values.rng_seed_enabled.SetValue(rng_seed_enabled);

    const auto custom_rtc_enabled = config->GetBoolean("System", "custom_rtc_enabled", false);
    if (custom_rtc_enabled) {
        Settings::values.custom_rtc = config->GetInteger("System", "custom_rtc", 0);
    } else {
        Settings::values.custom_rtc = 0;
    }
    Settings::values.custom_rtc_enabled = custom_rtc_enabled;

    ReadSetting("System", Settings::values.language_index);
    ReadSetting("System", Settings::values.region_index);
    ReadSetting("System", Settings::values.time_zone_index);
    ReadSetting("System", Settings::values.sound_index);

    // Core
    ReadSetting("Core", Settings::values.use_multi_core);
    ReadSetting("Core", Settings::values.memory_layout_mode);

    // Cpu
    ReadSetting("Cpu", Settings::values.cpu_accuracy);
    ReadSetting("Cpu", Settings::values.cpu_debug_mode);
    ReadSetting("Cpu", Settings::values.cpuopt_page_tables);
    ReadSetting("Cpu", Settings::values.cpuopt_block_linking);
    ReadSetting("Cpu", Settings::values.cpuopt_return_stack_buffer);
    ReadSetting("Cpu", Settings::values.cpuopt_fast_dispatcher);
    ReadSetting("Cpu", Settings::values.cpuopt_context_elimination);
    ReadSetting("Cpu", Settings::values.cpuopt_const_prop);
    ReadSetting("Cpu", Settings::values.cpuopt_misc_ir);
    ReadSetting("Cpu", Settings::values.cpuopt_reduce_misalign_checks);
    ReadSetting("Cpu", Settings::values.cpuopt_fastmem);
    ReadSetting("Cpu", Settings::values.cpuopt_fastmem_exclusives);
    ReadSetting("Cpu", Settings::values.cpuopt_recompile_exclusives);
    ReadSetting("Cpu", Settings::values.cpuopt_ignore_memory_aborts);
    ReadSetting("Cpu", Settings::values.cpuopt_unsafe_unfuse_fma);
    ReadSetting("Cpu", Settings::values.cpuopt_unsafe_reduce_fp_error);
    ReadSetting("Cpu", Settings::values.cpuopt_unsafe_ignore_standard_fpcr);
    ReadSetting("Cpu", Settings::values.cpuopt_unsafe_inaccurate_nan);
    ReadSetting("Cpu", Settings::values.cpuopt_unsafe_fastmem_check);
    ReadSetting("Cpu", Settings::values.cpuopt_unsafe_ignore_global_monitor);

    // Renderer
    ReadSetting("Renderer", Settings::values.renderer_backend);
    ReadSetting("Renderer", Settings::values.renderer_debug);
    ReadSetting("Renderer", Settings::values.renderer_shader_feedback);
    ReadSetting("Renderer", Settings::values.enable_nsight_aftermath);
    ReadSetting("Renderer", Settings::values.disable_shader_loop_safety_checks);
    ReadSetting("Renderer", Settings::values.vulkan_device);

    ReadSetting("Renderer", Settings::values.resolution_setup);
    ReadSetting("Renderer", Settings::values.scaling_filter);
    ReadSetting("Renderer", Settings::values.fsr_sharpening_slider);
    ReadSetting("Renderer", Settings::values.anti_aliasing);
    ReadSetting("Renderer", Settings::values.fullscreen_mode);
    ReadSetting("Renderer", Settings::values.aspect_ratio);
    ReadSetting("Renderer", Settings::values.max_anisotropy);
    ReadSetting("Renderer", Settings::values.use_speed_limit);
    ReadSetting("Renderer", Settings::values.speed_limit);
    ReadSetting("Renderer", Settings::values.use_disk_shader_cache);
    ReadSetting("Renderer", Settings::values.use_asynchronous_gpu_emulation);
    ReadSetting("Renderer", Settings::values.vsync_mode);
    ReadSetting("Renderer", Settings::values.shader_backend);
    ReadSetting("Renderer", Settings::values.use_asynchronous_shaders);
    ReadSetting("Renderer", Settings::values.nvdec_emulation);
    ReadSetting("Renderer", Settings::values.use_fast_gpu_time);
    ReadSetting("Renderer", Settings::values.use_vulkan_driver_pipeline_cache);

    ReadSetting("Renderer", Settings::values.bg_red);
    ReadSetting("Renderer", Settings::values.bg_green);
    ReadSetting("Renderer", Settings::values.bg_blue);

    // Use GPU accuracy normal by default on Android
    Settings::values.gpu_accuracy = static_cast<Settings::GpuAccuracy>(config->GetInteger(
        "Renderer", "gpu_accuracy", static_cast<u32>(Settings::GpuAccuracy::Normal)));

    // Use GPU default anisotropic filtering on Android
    Settings::values.max_anisotropy =
        static_cast<Settings::AnisotropyMode>(config->GetInteger("Renderer", "max_anisotropy", 1));

    // Disable ASTC compute by default on Android
    Settings::values.accelerate_astc.SetValue(
        config->GetBoolean("Renderer", "accelerate_astc", false) ? Settings::AstcDecodeMode::Gpu
                                                                 : Settings::AstcDecodeMode::Cpu);

    // Enable asynchronous presentation by default on Android
    Settings::values.async_presentation =
        config->GetBoolean("Renderer", "async_presentation", true);

    // Disable force_max_clock by default on Android
    Settings::values.renderer_force_max_clock =
        config->GetBoolean("Renderer", "force_max_clock", false);

    // Disable use_reactive_flushing by default on Android
    Settings::values.use_reactive_flushing =
        config->GetBoolean("Renderer", "use_reactive_flushing", false);

    // Audio
    ReadSetting("Audio", Settings::values.sink_id);
    ReadSetting("Audio", Settings::values.audio_output_device_id);
    ReadSetting("Audio", Settings::values.volume);

    // Miscellaneous
    // log_filter has a different default here than from common
    Settings::values.log_filter = "*:Info";
    ReadSetting("Miscellaneous", Settings::values.use_dev_keys);

    // Debugging
    Settings::values.record_frame_times =
        config->GetBoolean("Debugging", "record_frame_times", false);
    ReadSetting("Debugging", Settings::values.dump_exefs);
    ReadSetting("Debugging", Settings::values.dump_nso);
    ReadSetting("Debugging", Settings::values.enable_fs_access_log);
    ReadSetting("Debugging", Settings::values.reporting_services);
    ReadSetting("Debugging", Settings::values.quest_flag);
    ReadSetting("Debugging", Settings::values.use_debug_asserts);
    ReadSetting("Debugging", Settings::values.use_auto_stub);
    ReadSetting("Debugging", Settings::values.disable_macro_jit);
    ReadSetting("Debugging", Settings::values.disable_macro_hle);
    ReadSetting("Debugging", Settings::values.use_gdbstub);
    ReadSetting("Debugging", Settings::values.gdbstub_port);

    const auto title_list = config->Get("AddOns", "title_ids", "");
    std::stringstream ss(title_list);
    std::string line;
    while (std::getline(ss, line, '|')) {
        const auto title_id = std::strtoul(line.c_str(), nullptr, 16);
        const auto disabled_list = config->Get("AddOns", "disabled_" + line, "");

        std::stringstream inner_ss(disabled_list);
        std::string inner_line;
        std::vector<std::string> out;
        while (std::getline(inner_ss, inner_line, '|')) {
            out.push_back(inner_line);
        }

        Settings::values.disabled_addons.insert_or_assign(title_id, out);
    }

    // Web Service
    ReadSetting("WebService", Settings::values.enable_telemetry);
    ReadSetting("WebService", Settings::values.web_api_url);
    ReadSetting("WebService", Settings::values.yuzu_username);
    ReadSetting("WebService", Settings::values.yuzu_token);

    // Network
    ReadSetting("Network", Settings::values.network_interface);

    // Android
    ReadSetting("Android", AndroidSettings::values.picture_in_picture);
    ReadSetting("Android", AndroidSettings::values.screen_layout);
}

void Config::Initialize(const std::string& config_name) {
    const auto fs_config_loc = FS::GetYuzuPath(FS::YuzuPath::ConfigDir);
    const auto config_file = fmt::format("{}.ini", config_name);

    switch (type) {
    case ConfigType::GlobalConfig:
        config_loc = FS::PathToUTF8String(fs_config_loc / config_file);
        break;
    case ConfigType::PerGameConfig:
        config_loc = FS::PathToUTF8String(fs_config_loc / "custom" / FS::ToU8String(config_file));
        break;
    case ConfigType::InputProfile:
        config_loc = FS::PathToUTF8String(fs_config_loc / "input" / config_file);
        LoadINI(DefaultINI::android_config_file);
        return;
    }
    LoadINI(DefaultINI::android_config_file);
    ReadValues();
}
