// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QLabel>
#include <qnamespace.h>
#include "common/settings.h"
#include "core/core.h"
#include "ui_configure_graphics_advanced.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_graphics_advanced.h"
#include "yuzu/configuration/shared_translation.h"
#include "yuzu/configuration/shared_widget.h"

ConfigureGraphicsAdvanced::ConfigureGraphicsAdvanced(
    const Core::System& system_,
    std::shared_ptr<std::forward_list<ConfigurationShared::Tab*>> group,
    const ConfigurationShared::TranslationMap& translations_,
    const ConfigurationShared::ComboboxTranslationMap& combobox_translations_, QWidget* parent)
    : Tab(group, parent), ui{std::make_unique<Ui::ConfigureGraphicsAdvanced>()}, system{system_},
      translations{translations_}, combobox_translations{combobox_translations_} {

    ui->setupUi(this);

    SetConfiguration();

    checkbox_enable_compute_pipelines->setVisible(false);
}

ConfigureGraphicsAdvanced::~ConfigureGraphicsAdvanced() = default;

void ConfigureGraphicsAdvanced::SetConfiguration() {
    const bool runtime_lock = !system.IsPoweredOn();
    auto& layout = *ui->populate_target->layout();
    std::map<u32, QWidget*> hold{}; // A map will sort the data for us

    for (auto setting :
         Settings::values.linkage.by_category[Settings::Category::RendererAdvanced]) {
        if (!Settings::IsConfiguringGlobal() && !setting->Switchable()) {
            continue;
        }

        ConfigurationShared::Widget* widget = new ConfigurationShared::Widget(
            setting, translations, combobox_translations, this, runtime_lock, apply_funcs);

        if (!widget->Valid()) {
            delete widget;
            continue;
        }

        hold.emplace(setting->Id(), widget);

        // Keep track of enable_compute_pipelines so we can display it when needed
        if (setting->Id() == Settings::values.enable_compute_pipelines.Id()) {
            checkbox_enable_compute_pipelines = widget;
        }
    }
    for (const auto& [id, widget] : hold) {
        layout.addWidget(widget);
    }
}

void ConfigureGraphicsAdvanced::ApplyConfiguration() {
    const bool is_powered_on = system.IsPoweredOn();
    for (const auto& func : apply_funcs) {
        func(is_powered_on);
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

void ConfigureGraphicsAdvanced::ExposeComputeOption() {
    checkbox_enable_compute_pipelines->setVisible(true);
}
