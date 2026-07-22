// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>
#include <QWidget>
#include "yuzu/configuration/configuration_shared.h"

namespace Core {
class System;
}

namespace Ui {
class ConfigureGraphicsExtensions;
}

namespace ConfigurationShared {
class Builder;
}

class ConfigureGraphicsExtensions : public ConfigurationShared::Tab {
    Q_OBJECT

public:
    explicit ConfigureGraphicsExtensions(
        const Core::System& system_, std::shared_ptr<std::vector<ConfigurationShared::Tab*>> group,
        const ConfigurationShared::Builder& builder, QWidget* parent = nullptr);
    ~ConfigureGraphicsExtensions() override;

    void ApplyConfiguration() override;
    void SetConfiguration() override;

private:
    void Setup(const ConfigurationShared::Builder& builder);
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    std::unique_ptr<Ui::ConfigureGraphicsExtensions> ui;

    const Core::System& system;

    std::vector<std::function<void(bool)>> apply_funcs;
};
