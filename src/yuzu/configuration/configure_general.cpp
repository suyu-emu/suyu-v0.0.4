// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <functional>
#include <utility>
#include <vector>
#include <QDir>
#include <QFileDialog>
#include <QListWidget>
#include <QMessageBox>
#include "common/settings.h"
#include "core/core.h"
#include "ui_configure_general.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_general.h"
#include "yuzu/configuration/shared_widget.h"
#include "qt_common/config/uisettings.h"

ConfigureGeneral::ConfigureGeneral(const Core::System& system_,
                                   std::shared_ptr<std::vector<ConfigurationShared::Tab*>> group_,
                                   const ConfigurationShared::Builder& builder, QWidget* parent)
    : Tab(group_, parent), ui{std::make_unique<Ui::ConfigureGeneral>()}, system{system_} {
    ui->setupUi(this);

    Setup(builder);

    SetConfiguration();

    connect(ui->button_reset_defaults, &QPushButton::clicked, this,
            &ConfigureGeneral::ResetDefaults);

    connect(ui->add_external_dir_button, &QPushButton::pressed, this,
            &ConfigureGeneral::AddExternalContentDirectory);
    connect(ui->remove_external_dir_button, &QPushButton::pressed, this,
            &ConfigureGeneral::RemoveSelectedExternalContentDirectory);
    connect(ui->external_content_list, &QListWidget::itemSelectionChanged, this, [this] {
        ui->remove_external_dir_button->setEnabled(
            !ui->external_content_list->selectedItems().isEmpty());
    });

    if (!Settings::IsConfiguringGlobal()) {
        ui->button_reset_defaults->setVisible(false);
    }
}

ConfigureGeneral::~ConfigureGeneral() = default;

void ConfigureGeneral::SetConfiguration() {
    UpdateExternalContentList();
}

void ConfigureGeneral::Setup(const ConfigurationShared::Builder& builder) {
    QLayout& general_layout = *ui->general_widget->layout();
    std::map<u32, QWidget*> general_hold{};
    std::map<u32, QWidget*> linux_hold{};

    std::vector<Settings::BasicSetting*> settings;

    auto push = [&settings](auto& list) {
        for (auto setting : list) {
            settings.push_back(setting);
        }
    };

    push(UISettings::values.linkage.by_category[Settings::Category::UiGeneral]);
    for (const auto setting : settings) {
        auto* widget = builder.BuildWidget(setting, apply_funcs);

        if (widget == nullptr) {
            continue;
        }
        if (!widget->Valid()) {
            widget->deleteLater();
            continue;
        }

        switch (setting->GetCategory()) {
        case Settings::Category::UiGeneral:
            general_hold.emplace(setting->Id(), widget);
            break;
        default:
            widget->deleteLater();
        }
    }

    for (const auto& [id, widget] : general_hold) {
        general_layout.addWidget(widget);
    }
}

// Called to set the callback when resetting settings to defaults
void ConfigureGeneral::SetResetCallback(std::function<void()> callback) {
    reset_callback = std::move(callback);
}

void ConfigureGeneral::ResetDefaults() {
    QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Eden"),
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

    std::vector<std::string> new_dirs;
    new_dirs.reserve(ui->external_content_list->count());
    for (int i = 0; i < ui->external_content_list->count(); ++i) {
        new_dirs.push_back(ui->external_content_list->item(i)->text().toStdString());
    }

    if (new_dirs != Settings::values.external_content_dirs) {
        Settings::values.external_content_dirs = std::move(new_dirs);
        emit ExternalContentDirsChanged();
    }
}

void ConfigureGeneral::UpdateExternalContentList() {
    ui->external_content_list->clear();
    for (const auto& dir : Settings::values.external_content_dirs) {
        ui->external_content_list->addItem(QString::fromStdString(dir));
    }
}

void ConfigureGeneral::AddExternalContentDirectory() {
    const QString dir_path = QFileDialog::getExistingDirectory(
        this, tr("Select External Content Directory..."), QString());

    if (dir_path.isEmpty()) {
        return;
    }

    QString normalized_path = QDir::toNativeSeparators(dir_path);
    if (normalized_path.back() != QDir::separator()) {
        normalized_path.append(QDir::separator());
    }

    for (int i = 0; i < ui->external_content_list->count(); ++i) {
        if (ui->external_content_list->item(i)->text() == normalized_path) {
            QMessageBox::information(this, tr("Directory Already Added"),
                                     tr("This directory is already in the list."));
            return;
        }
    }

    ui->external_content_list->addItem(normalized_path);
}

void ConfigureGeneral::RemoveSelectedExternalContentDirectory() {
    auto selected = ui->external_content_list->selectedItems();
    if (!selected.isEmpty()) {
        qDeleteAll(ui->external_content_list->selectedItems());
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
