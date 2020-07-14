// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_graphics_advanced.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_graphics_advanced.h"

ConfigureGraphicsAdvanced::ConfigureGraphicsAdvanced(QWidget* parent)
    : QWidget(parent), ui(new Ui::ConfigureGraphicsAdvanced) {

    ui->setupUi(this);

    SetupPerGameUI();

    SetConfiguration();
}

ConfigureGraphicsAdvanced::~ConfigureGraphicsAdvanced() = default;

void ConfigureGraphicsAdvanced::SetConfiguration() {
    const bool runtime_lock = !Core::System::GetInstance().IsPoweredOn();
    ui->use_vsync->setEnabled(runtime_lock);
    ui->use_assembly_shaders->setEnabled(runtime_lock);
    ui->force_30fps_mode->setEnabled(runtime_lock);
    ui->anisotropic_filtering_combobox->setEnabled(runtime_lock);

    if (Settings::configuring_global) {
        ui->gpu_accuracy->setCurrentIndex(
            static_cast<int>(Settings::values.gpu_accuracy.GetValue()));
        ui->use_vsync->setChecked(Settings::values.use_vsync.GetValue());
        ui->use_assembly_shaders->setChecked(Settings::values.use_assembly_shaders.GetValue());
        ui->use_fast_gpu_time->setChecked(Settings::values.use_fast_gpu_time.GetValue());
        ui->force_30fps_mode->setChecked(Settings::values.force_30fps_mode.GetValue());
        ui->anisotropic_filtering_combobox->setCurrentIndex(
            Settings::values.max_anisotropy.GetValue());
    } else {
        ConfigurationShared::SetPerGameSetting(ui->gpu_accuracy, &Settings::values.gpu_accuracy);
        ConfigurationShared::SetPerGameSetting(ui->use_vsync, &Settings::values.use_vsync);
        ConfigurationShared::SetPerGameSetting(ui->use_assembly_shaders,
                                               &Settings::values.use_assembly_shaders);
        ConfigurationShared::SetPerGameSetting(ui->use_fast_gpu_time,
                                               &Settings::values.use_fast_gpu_time);
        ConfigurationShared::SetPerGameSetting(ui->force_30fps_mode,
                                               &Settings::values.force_30fps_mode);
        ConfigurationShared::SetPerGameSetting(ui->anisotropic_filtering_combobox,
                                               &Settings::values.max_anisotropy);
    }
}

void ConfigureGraphicsAdvanced::ApplyConfiguration() {
    // Subtract 2 if configuring per-game (separator and "use global configuration" take 2 slots)
    const auto gpu_accuracy = static_cast<Settings::GPUAccuracy>(
        ui->gpu_accuracy->currentIndex() -
        ((Settings::configuring_global) ? 0 : ConfigurationShared::USE_GLOBAL_OFFSET));

    if (Settings::configuring_global) {
        // Must guard in case of a during-game configuration when set to be game-specific.
        if (Settings::values.gpu_accuracy.UsingGlobal()) {
            Settings::values.gpu_accuracy.SetValue(gpu_accuracy);
        }
        if (Settings::values.use_vsync.UsingGlobal()) {
            Settings::values.use_vsync.SetValue(ui->use_vsync->isChecked());
        }
        if (Settings::values.use_assembly_shaders.UsingGlobal()) {
            Settings::values.use_assembly_shaders.SetValue(ui->use_assembly_shaders->isChecked());
        }
        if (Settings::values.use_fast_gpu_time.UsingGlobal()) {
            Settings::values.use_fast_gpu_time.SetValue(ui->use_fast_gpu_time->isChecked());
        }
        if (Settings::values.force_30fps_mode.UsingGlobal()) {
            Settings::values.force_30fps_mode.SetValue(ui->force_30fps_mode->isChecked());
        }
        if (Settings::values.max_anisotropy.UsingGlobal()) {
            Settings::values.max_anisotropy.SetValue(
                ui->anisotropic_filtering_combobox->currentIndex());
        }
    } else {
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.max_anisotropy,
                                                 ui->anisotropic_filtering_combobox);
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_vsync, ui->use_vsync);
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_assembly_shaders,
                                                 ui->use_assembly_shaders);
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_fast_gpu_time,
                                                 ui->use_fast_gpu_time);
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.force_30fps_mode,
                                                 ui->force_30fps_mode);
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.max_anisotropy,
                                                 ui->anisotropic_filtering_combobox);

        if (ui->gpu_accuracy->currentIndex() == ConfigurationShared::USE_GLOBAL_INDEX) {
            Settings::values.gpu_accuracy.SetGlobal(true);
        } else {
            Settings::values.gpu_accuracy.SetGlobal(false);
            Settings::values.gpu_accuracy.SetValue(gpu_accuracy);
        }
    }
}

void ConfigureGraphicsAdvanced::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureGraphicsAdvanced::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureGraphicsAdvanced::SetupPerGameUI() {
    // Disable if not global (only happens during game)
    if (Settings::configuring_global) {
        ui->gpu_accuracy->setEnabled(Settings::values.gpu_accuracy.UsingGlobal());
        ui->use_vsync->setEnabled(Settings::values.use_vsync.UsingGlobal());
        ui->use_assembly_shaders->setEnabled(Settings::values.use_assembly_shaders.UsingGlobal());
        ui->use_fast_gpu_time->setEnabled(Settings::values.use_fast_gpu_time.UsingGlobal());
        ui->force_30fps_mode->setEnabled(Settings::values.force_30fps_mode.UsingGlobal());
        ui->anisotropic_filtering_combobox->setEnabled(
            Settings::values.max_anisotropy.UsingGlobal());

        return;
    }

    ConfigurationShared::InsertGlobalItem(ui->gpu_accuracy);
    ui->use_vsync->setTristate(true);
    ui->use_assembly_shaders->setTristate(true);
    ui->use_fast_gpu_time->setTristate(true);
    ui->force_30fps_mode->setTristate(true);
    ConfigurationShared::InsertGlobalItem(ui->anisotropic_filtering_combobox);
}
