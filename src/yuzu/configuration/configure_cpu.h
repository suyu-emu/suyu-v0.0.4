// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <QWidget>
#include "yuzu/configuration/configuration_shared.h"

namespace Core {
class System;
}

namespace Ui {
class ConfigureCpu;
}

class ConfigureCpu : public ConfigurationShared::Tab {
public:
    explicit ConfigureCpu(const Core::System& system_,
                          std::shared_ptr<std::forward_list<ConfigurationShared::Tab*>> group,
                          const ConfigurationShared::TranslationMap& translations,
                          const ConfigurationShared::ComboboxTranslationMap& combobox_translations,
                          QWidget* parent = nullptr);
    ~ConfigureCpu() override;

    void ApplyConfiguration() override;
    void SetConfiguration() override;

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void UpdateGroup(int index);

    void Setup();

    std::unique_ptr<Ui::ConfigureCpu> ui;

    ConfigurationShared::CheckState cpuopt_unsafe_unfuse_fma;
    ConfigurationShared::CheckState cpuopt_unsafe_reduce_fp_error;
    ConfigurationShared::CheckState cpuopt_unsafe_ignore_standard_fpcr;
    ConfigurationShared::CheckState cpuopt_unsafe_inaccurate_nan;
    ConfigurationShared::CheckState cpuopt_unsafe_fastmem_check;
    ConfigurationShared::CheckState cpuopt_unsafe_ignore_global_monitor;

    const Core::System& system;

    const ConfigurationShared::TranslationMap& translations;
    const ConfigurationShared::ComboboxTranslationMap& combobox_translations;

    std::forward_list<std::function<void(bool)>> apply_funcs{};

    QComboBox* accuracy_combobox;
};
