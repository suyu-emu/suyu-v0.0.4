// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>
#include "core/settings.h"

namespace Ui {
class ConfigureCpu;
}

class ConfigureCpu : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureCpu(QWidget* parent = nullptr);
    ~ConfigureCpu() override;

    void ApplyConfiguration();

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void AccuracyUpdated(int index);
    void UpdateGroup(int index);

    void SetConfiguration();

    std::unique_ptr<Ui::ConfigureCpu> ui;
};
