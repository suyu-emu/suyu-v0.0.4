// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>
#include "yuzu/configuration/configure_input.h"

class QPushButton;

namespace InputCommon {
class InputSubsystem;
}

namespace Ui {
class ConfigureInputDialog;
}

class ConfigureInputDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConfigureInputDialog(QWidget* parent, std::size_t max_players,
                                  InputCommon::InputSubsystem* input_subsystem);
    ~ConfigureInputDialog() override;

    void ApplyConfiguration();

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    std::unique_ptr<Ui::ConfigureInputDialog> ui;

    ConfigureInput* input_widget;
};
