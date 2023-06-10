// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <forward_list>
#include <memory>
#include <typeinfo>
#include <QComboBox>
#include "common/common_types.h"
#include "common/settings.h"
#include "configuration/shared_widget.h"
#include "core/core.h"
#include "ui_configure_cpu.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_cpu.h"

ConfigureCpu::ConfigureCpu(
    const Core::System& system_,
    std::shared_ptr<std::forward_list<ConfigurationShared::Tab*>> group,
    const ConfigurationShared::TranslationMap& translations_,
    const ConfigurationShared::ComboboxTranslationMap& combobox_translations_, QWidget* parent)
    : Tab(group, parent), ui{std::make_unique<Ui::ConfigureCpu>()}, system{system_},
      translations{translations_}, combobox_translations{combobox_translations_} {
    ui->setupUi(this);

    Setup();

    SetConfiguration();

    connect(accuracy_combobox, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ConfigureCpu::UpdateGroup);
}

ConfigureCpu::~ConfigureCpu() = default;

void ConfigureCpu::SetConfiguration() {}
void ConfigureCpu::Setup() {
    const bool runtime_lock = !system.IsPoweredOn();
    auto* accuracy_layout = ui->widget_accuracy->layout();
    auto* unsafe_layout = ui->unsafe_widget->layout();
    std::map<std::string, QWidget*> unsafe_hold{};

    std::forward_list<Settings::BasicSetting*> settings;
    const auto push = [&](Settings::Category category) {
        for (const auto setting : Settings::values.linkage.by_category[category]) {
            settings.push_front(setting);
        }
    };

    push(Settings::Category::Cpu);
    push(Settings::Category::CpuUnsafe);

    for (const auto setting : settings) {
        if (!Settings::IsConfiguringGlobal() && !setting->Switchable()) {
            continue;
        }

        auto* widget = new ConfigurationShared::Widget(setting, translations, combobox_translations,
                                                       this, runtime_lock, apply_funcs);

        if (!widget->Valid()) {
            delete widget;
            continue;
        }

        if (setting->Id() == Settings::values.cpu_accuracy.Id()) {
            // Keep track of cpu_accuracy combobox to display/hide the unsafe settings
            accuracy_layout->addWidget(widget);
            accuracy_combobox = widget->combobox;
        } else {
            // Presently, all other settings here are unsafe checkboxes
            unsafe_hold.insert({setting->GetLabel(), widget});
        }
    }

    for (const auto& [label, widget] : unsafe_hold) {
        unsafe_layout->addWidget(widget);
    }

    UpdateGroup(accuracy_combobox->currentIndex());
}

void ConfigureCpu::UpdateGroup(int index) {
    const auto accuracy = static_cast<Settings::CpuAccuracy>(
        combobox_translations.at(typeid(Settings::CpuAccuracy))[index].first);
    ui->unsafe_group->setVisible(accuracy == Settings::CpuAccuracy::Unsafe);
}

void ConfigureCpu::ApplyConfiguration() {
    const bool is_powered_on = system.IsPoweredOn();
    for (const auto& apply_func : apply_funcs) {
        apply_func(is_powered_on);
    }
}

void ConfigureCpu::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureCpu::RetranslateUI() {
    ui->retranslateUi(this);
}
