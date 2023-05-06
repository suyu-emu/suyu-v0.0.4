// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <QWidget>
#include "yuzu/configuration/configuration_shared.h"

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
    void SetVolumeIndicatorText(int percentage);

    void SetupPerGameUI();

    std::unique_ptr<Ui::ConfigureAudio> ui;

    const Core::System& system;
};
