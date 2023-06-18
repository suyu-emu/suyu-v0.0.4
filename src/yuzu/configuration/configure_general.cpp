// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <functional>
#include <utility>
#include <QMessageBox>
#include "common/settings.h"
#include "core/core.h"
#include "ui_configure_general.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_general.h"
#include "yuzu/configuration/shared_widget.h"
#include "yuzu/uisettings.h"

ConfigureGeneral::ConfigureGeneral(
    const Core::System& system_,
    std::shared_ptr<std::forward_list<ConfigurationShared::Tab*>> group_,
    const ConfigurationShared::TranslationMap& translations_,
    const ConfigurationShared::ComboboxTranslationMap& combobox_translations_, QWidget* parent)
    : Tab(group_, parent), ui{std::make_unique<Ui::ConfigureGeneral>()}, system{system_},
      translations{translations_}, combobox_translations{combobox_translations_} {
    ui->setupUi(this);

    SetConfiguration();

    connect(ui->button_reset_defaults, &QPushButton::clicked, this,
            &ConfigureGeneral::ResetDefaults);

    if (!Settings::IsConfiguringGlobal()) {
        ui->button_reset_defaults->setVisible(false);
    }
}

ConfigureGeneral::~ConfigureGeneral() = default;

void ConfigureGeneral::SetConfiguration() {
    const bool runtime_lock = !system.IsPoweredOn();
    QLayout& layout = *ui->general_widget->layout();

    std::map<u32, QWidget*> hold{};

    for (const auto setting :
         UISettings::values.linkage.by_category[Settings::Category::UiGeneral]) {
        auto* widget = new ConfigurationShared::Widget(setting, translations, combobox_translations,
                                                       this, runtime_lock, apply_funcs);

        if (!widget->Valid()) {
            delete widget;
            continue;
        }

        hold.emplace(setting->Id(), widget);
    }

    for (const auto& [id, widget] : hold) {
        layout.addWidget(widget);
    }
}

// Called to set the callback when resetting settings to defaults
void ConfigureGeneral::SetResetCallback(std::function<void()> callback) {
    reset_callback = std::move(callback);
}

void ConfigureGeneral::ResetDefaults() {
    QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("yuzu"),
        tr("This reset all settings and remove all per-game configurations. This will not delete "
           "game directories, profiles, or input profiles. Proceed?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::No) {
        return;
    }
    UISettings::values.reset_to_defaults = true;
    UISettings::values.is_game_list_reload_pending.exchange(true);
    reset_callback();
}

void ConfigureGeneral::ApplyConfiguration() {
    bool powered_on = system.IsPoweredOn();
    for (const auto& func : apply_funcs) {
        func(powered_on);
    }
}

void ConfigureGeneral::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureGeneral::RetranslateUI() {
    ui->retranslateUi(this);
}
