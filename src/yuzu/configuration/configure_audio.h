// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <forward_list>
#include <functional>
#include <memory>
#include <QWidget>
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/shared_translation.h"

class QPushButton;

namespace Core {
class System;
}

namespace Ui {
class ConfigureAudio;
}

class ConfigureAudio : public ConfigurationShared::Tab {
public:
    explicit ConfigureAudio(const Core::System& system_,
                            std::shared_ptr<std::forward_list<ConfigurationShared::Tab*>> group,
                            const ConfigurationShared::TranslationMap& translations_,
                            QWidget* parent = nullptr);
    ~ConfigureAudio() override;

    void ApplyConfiguration() override;
    void SetConfiguration() override;

private:
    void changeEvent(QEvent* event) override;

    void InitializeAudioSinkComboBox();

    void RetranslateUI();

    void UpdateAudioDevices(int sink_index);

    void SetOutputSinkFromSinkID();
    void SetAudioDevicesFromDeviceID();

    void Setup();

    std::unique_ptr<Ui::ConfigureAudio> ui;

    const Core::System& system;
    const ConfigurationShared::TranslationMap& translations;

    std::forward_list<std::function<void(bool)>> apply_funcs{};

    QComboBox* sink_combo_box;
    QComboBox* output_device_combo_box;
    QComboBox* input_device_combo_box;
};
