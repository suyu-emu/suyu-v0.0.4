// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

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
};
