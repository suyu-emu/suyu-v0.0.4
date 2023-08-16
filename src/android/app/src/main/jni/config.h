// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "common/settings.h"

class INIReader;

class Config {
    bool LoadINI(const std::string& default_contents = "", bool retry = true);

public:
    enum class ConfigType {
        GlobalConfig,
        PerGameConfig,
        InputProfile,
    };

    explicit Config(const std::string& config_name = "config",
                    ConfigType config_type = ConfigType::GlobalConfig);
    ~Config();

    void Initialize(const std::string& config_name);

private:
    /**
     * Applies a value read from the config to a Setting.
     *
     * @param group The name of the INI group
     * @param setting The yuzu setting to modify
     */
    template <typename Type, bool ranged>
    void ReadSetting(const std::string& group, Settings::Setting<Type, ranged>& setting);

    void ReadValues();

    const ConfigType type;
    std::unique_ptr<INIReader> config;
    std::string config_loc;
    const bool global;
};
