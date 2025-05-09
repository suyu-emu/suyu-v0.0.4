// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <vector>
#include <QLabel>
#include <qnamespace.h>
#include <QCheckBox>
#include <QSlider>
#include "common/settings.h"
#include "core/core.h"
#include "ui_configure_graphics_extensions.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_graphics_extensions.h"
#include "yuzu/configuration/shared_translation.h"
#include "yuzu/configuration/shared_widget.h"

ConfigureGraphicsExtensions::ConfigureGraphicsExtensions(
    const Core::System& system_, std::shared_ptr<std::vector<ConfigurationShared::Tab*>> group_,
    const ConfigurationShared::Builder& builder, QWidget* parent)
    : Tab(group_, parent), ui{std::make_unique<Ui::ConfigureGraphicsExtensions>()}, system{system_} {

    ui->setupUi(this);

    Setup(builder);

    SetConfiguration();
}

ConfigureGraphicsExtensions::~ConfigureGraphicsExtensions() = default;

void ConfigureGraphicsExtensions::SetConfiguration() {}

void ConfigureGraphicsExtensions::Setup(const ConfigurationShared::Builder& builder) {
    auto& layout = *ui->populate_target->layout();
    std::map<u32, QWidget*> hold{}; // A map will sort the data for us

    QSlider *dyna_state = nullptr;
    QCheckBox *dyna_state3 = nullptr;
    for (auto setting :
         Settings::values.linkage.by_category[Settings::Category::RendererExtensions]) {
        ConfigurationShared::Widget* widget = builder.BuildWidget(setting, apply_funcs);

        if (widget == nullptr) {
            continue;
        }
        if (!widget->Valid()) {
            widget->deleteLater();
            continue;
        }

        hold.emplace(setting->Id(), widget);

        if (setting->Id() == Settings::values.dyna_state.Id()) {
            widget->slider->setTickInterval(1);
            widget->slider->setTickPosition(QSlider::TicksAbove);
            dyna_state = widget->slider;
        } else if (setting->Id() == Settings::values.dyna_state3.Id()) {
            dyna_state3 = widget->checkbox;
        }
    }

    for (const auto& [id, widget] : hold) {
        layout.addWidget(widget);
    }

    connect(dyna_state, &QSlider::sliderMoved, this, [dyna_state3](int value) {
        dyna_state3->setEnabled(value == 2);
        if (value < 2) {
            dyna_state3->setChecked(false);
        }
    });
}

void ConfigureGraphicsExtensions::ApplyConfiguration() {
    const bool is_powered_on = system.IsPoweredOn();
    for (const auto& func : apply_funcs) {
        func(is_powered_on);
    }
}

void ConfigureGraphicsExtensions::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureGraphicsExtensions::RetranslateUI() {
    ui->retranslateUi(this);
}
