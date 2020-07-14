// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>
#include "yuzu/configuration/configuration_shared.h"

namespace Ui {
class ConfigureGraphicsAdvanced;
}

class ConfigureGraphicsAdvanced : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureGraphicsAdvanced(QWidget* parent = nullptr);
    ~ConfigureGraphicsAdvanced() override;

    void ApplyConfiguration();

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void SetConfiguration();

    void SetupPerGameUI();

    std::unique_ptr<Ui::ConfigureGraphicsAdvanced> ui;

    struct Trackers {
        ConfigurationShared::CheckState use_vsync;
        ConfigurationShared::CheckState use_assembly_shaders;
        ConfigurationShared::CheckState use_asynchronous_shaders;
        ConfigurationShared::CheckState use_fast_gpu_time;
        ConfigurationShared::CheckState force_30fps_mode;
    } trackers;
};
