// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QList>
#include <QWidget>
#include "yuzu/configuration/configuration_shared.h"

namespace Ui {
class ConfigureSystem;
}

class ConfigureSystem : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureSystem(QWidget* parent = nullptr);
    ~ConfigureSystem() override;

    void ApplyConfiguration();

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void SetConfiguration();

    void ReadSystemSettings();

    void RefreshConsoleID();

    void SetupPerGameUI();

    std::unique_ptr<Ui::ConfigureSystem> ui;
    bool enabled = false;

    int language_index = 0;
    int region_index = 0;
    int time_zone_index = 0;
    int sound_index = 0;

    struct Trackers {
        ConfigurationShared::CheckState use_rng_seed;
        ConfigurationShared::CheckState use_custom_rtc;
    } trackers;
};
