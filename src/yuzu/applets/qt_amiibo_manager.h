// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <QDialog>
#include "core/frontend/applets/cabinet.h"

class GMainWindow;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;

class InputProfiles;

namespace InputCommon {
class InputSubsystem;
}

namespace Ui {
class QtAmiiboManagerDialog;
}

namespace Core {
class System;
}

namespace Core::HID {
class HIDCore;
enum class NpadStyleIndex : u8;
} // namespace Core::HID

namespace Service::NFP {
class NfpDevice;
} // namespace Service::NFP

class QtAmiiboManagerDialog final : public QDialog {
    Q_OBJECT

public:
    explicit QtAmiiboManagerDialog(QWidget* parent, Core::Frontend::CabinetParameters parameters_,
                                   InputCommon::InputSubsystem* input_subsystem_,
                                   std::shared_ptr<Service::NFP::NfpDevice> nfp_device_);
    ~QtAmiiboManagerDialog() override;

    int exec() override;

    std::string GetName();

private:
    void LoadInfo();
    void LoadAmiiboApiInfo(std::string amiibo_id);
    void LoadAmiiboData();
    void LoadAmiiboGameInfo();
    void SetGameDataName(u32 application_area_id);
    void SetManagerDescription();

    std::unique_ptr<Ui::QtAmiiboManagerDialog> ui;

    InputCommon::InputSubsystem* input_subsystem;
    std::shared_ptr<Service::NFP::NfpDevice> nfp_device;

    // Parameters sent in from the backend HLE applet.
    Core::Frontend::CabinetParameters parameters;

    // If false amiibo manager failed to load
    bool is_initalized{};
};

class QtAmiiboManager final : public QObject, public Core::Frontend::CabinetApplet {
    Q_OBJECT

public:
    explicit QtAmiiboManager(GMainWindow& parent);
    ~QtAmiiboManager() override;

    void ShowCabinetApplet(std::function<void(bool, const std::string&)> callback_,
                           const Core::Frontend::CabinetParameters& parameters,
                           std::shared_ptr<Service::NFP::NfpDevice> nfp_device) const override;

signals:
    void MainWindowShowAmiiboManager(const Core::Frontend::CabinetParameters& parameters,
                                     std::shared_ptr<Service::NFP::NfpDevice> nfp_device) const;

private:
    void MainWindowFinished(bool is_success, std::string name);

    mutable std::function<void(bool, const std::string&)> callback;
};
