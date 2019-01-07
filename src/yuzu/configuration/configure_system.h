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

    void applyConfiguration();
    void setConfiguration();

private:
    void ReadSystemSettings();

    void UpdateBirthdayComboBox(int birthmonth_index);
    void RefreshConsoleID();

    std::unique_ptr<Ui::ConfigureSystem> ui;
    bool enabled = false;

    int birthmonth = 0;
    int birthday = 0;
    int language_index = 0;
    int sound_index = 0;
};
