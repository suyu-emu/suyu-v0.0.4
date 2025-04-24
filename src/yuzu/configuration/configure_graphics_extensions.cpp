// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <vector>
#include <QLabel>
#include <qnamespace.h>
#include <QCheckBox>
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

    QCheckBox *dyna_state_1_box = nullptr;
    QCheckBox *dyna_state_2_box = nullptr;
    QCheckBox *dyna_state_2_extras_box = nullptr;
    QCheckBox *dyna_state_3_box = nullptr;
    QCheckBox *dyna_state_3_blend_box = nullptr;

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

#define CHECKBOX(state) if (setting->Id() == Settings::values.use_dyna_state_##state.Id()) { \
        dyna_state_##state##_box = widget->checkbox; \
    } else

        CHECKBOX(1)
        CHECKBOX(2)
        CHECKBOX(2_extras)
        CHECKBOX(3)
        CHECKBOX(3_blend)
        {} // else
    }

    for (const auto& [id, widget] : hold) {
        layout.addWidget(widget);
    }

    // I hate everything about this
    auto state_1_check = [=](int state) {
        bool checked = state == (int) Qt::CheckState::Checked;

        if (!checked) dyna_state_2_box->setChecked(false);
        dyna_state_2_box->setEnabled(checked);
    };

    connect(dyna_state_1_box, &QCheckBox::stateChanged, this, state_1_check);

    auto state_2_check = [=](int state) {
        bool checked = state == (int) Qt::CheckState::Checked;
        bool valid = dyna_state_1_box->isChecked();

        if (!valid) {
            // THIS IS SO BAD
            if (!checked) {
                emit dyna_state_2_box->clicked();
            } else {
                checked = false;
                dyna_state_2_box->setChecked(false);
                return;
            }
        }

        if (!checked) {
            dyna_state_2_extras_box->setChecked(false);
            dyna_state_3_box->setChecked(false);
        }

        dyna_state_2_extras_box->setEnabled(checked);
        dyna_state_3_box->setEnabled(checked);
    };

    connect(dyna_state_2_box, &QCheckBox::stateChanged, this, state_2_check);

    auto state_3_check = [=](int state) {
        bool checked = state == (int) Qt::CheckState::Checked;
        bool valid = dyna_state_2_box->isChecked();

        if (!valid) {
            if (!checked) {
                emit dyna_state_3_box->clicked();
            } else {
                checked = false;
                dyna_state_3_box->setChecked(false);
                return;
            }
        }

        if (!checked) dyna_state_3_blend_box->setChecked(false);
        dyna_state_3_blend_box->setEnabled(checked);
    };

    connect(dyna_state_3_box, &QCheckBox::stateChanged, this, state_3_check);

    auto state_2_extras_check = [=](int state) {
        bool checked = state == (int) Qt::CheckState::Checked;
        bool valid = dyna_state_2_box->isChecked();

        if (!valid) {
            if (!checked) {
                emit dyna_state_2_extras_box->clicked();
            } else {
                checked = false;
                dyna_state_2_extras_box->setChecked(false);
                return;
            }
        }
    };

    connect(dyna_state_2_extras_box, &QCheckBox::stateChanged, this, state_2_extras_check);

    auto state_3_blend_check = [=](int state) {
        bool checked = state == (int) Qt::CheckState::Checked;
        bool valid = dyna_state_3_box->isChecked();

        if (!valid) {
            if (!checked) {
                emit dyna_state_3_blend_box->clicked();
            } else {
                checked = false;
                dyna_state_3_blend_box->setChecked(false);
                return;
            }
        }
    };

    connect(dyna_state_3_blend_box, &QCheckBox::stateChanged, this, state_3_blend_check);

    state_1_check((int) dyna_state_1_box->checkState());
    state_2_check((int) dyna_state_2_box->checkState());
    state_2_extras_check((int) dyna_state_2_extras_box->checkState());
    state_3_check((int) dyna_state_3_box->checkState());
    state_3_blend_check((int) dyna_state_3_blend_box->checkState());
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
