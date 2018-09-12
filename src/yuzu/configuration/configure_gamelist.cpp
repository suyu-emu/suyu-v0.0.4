// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <utility>

#include "common/common_types.h"
#include "core/settings.h"
#include "ui_configure_gamelist.h"
#include "yuzu/configuration/configure_gamelist.h"
#include "yuzu/ui_settings.h"

ConfigureGameList::ConfigureGameList(QWidget* parent)
    : QWidget(parent), ui(new Ui::ConfigureGameList) {
    ui->setupUi(this);

    InitializeIconSizeComboBox();
    InitializeRowComboBoxes();

    this->setConfiguration();
}

ConfigureGameList::~ConfigureGameList() = default;

void ConfigureGameList::applyConfiguration() {
    UISettings::values.show_unknown = ui->show_unknown->isChecked();
    UISettings::values.icon_size = ui->icon_size_combobox->currentData().toUInt();
    UISettings::values.row_1_text_id = ui->row_1_text_combobox->currentData().toUInt();
    UISettings::values.row_2_text_id = ui->row_2_text_combobox->currentData().toUInt();
    Settings::Apply();
}

void ConfigureGameList::setConfiguration() {
    ui->show_unknown->setChecked(UISettings::values.show_unknown);
    ui->icon_size_combobox->setCurrentIndex(
        ui->icon_size_combobox->findData(UISettings::values.icon_size));
    ui->row_1_text_combobox->setCurrentIndex(
        ui->row_1_text_combobox->findData(UISettings::values.row_1_text_id));
    ui->row_2_text_combobox->setCurrentIndex(
        ui->row_2_text_combobox->findData(UISettings::values.row_2_text_id));
}

void ConfigureGameList::InitializeIconSizeComboBox() {
    static const std::array<std::pair<u32, std::string>, 5> default_icon_sizes{{
        std::make_pair(0, "None"),        std::make_pair(32, "Small"),
        std::make_pair(64, "Standard"),   std::make_pair(128, "Large"),
        std::make_pair(256, "Full Size"),
    }};

    for (const auto& size : default_icon_sizes) {
        ui->icon_size_combobox->addItem(QString::fromStdString(size.second + " (" +
                                                               std::to_string(size.first) + "x" +
                                                               std::to_string(size.first) + ")"),
                                        size.first);
    }
}

void ConfigureGameList::InitializeRowComboBoxes() {
    static const std::array<std::string, 4> row_text_names{{
        "Filename",
        "Filetype",
        "Title ID",
        "Title Name",
    }};

    for (size_t i = 0; i < row_text_names.size(); ++i) {
        ui->row_1_text_combobox->addItem(QString::fromStdString(row_text_names[i]),
                                         QVariant::fromValue(i));
        ui->row_2_text_combobox->addItem(QString::fromStdString(row_text_names[i]),
                                         QVariant::fromValue(i));
    }
}
