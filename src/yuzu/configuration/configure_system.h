// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QList>
#include <QWidget>

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

    std::unique_ptr<Ui::ConfigureSystem> ui;
    bool enabled = false;

    int language_index = 0;
    int region_index = 0;
    int time_zone_index = 0;
    int sound_index = 0;
};
