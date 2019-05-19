// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <chrono>
#include <optional>

#include <QFileDialog>
#include <QGraphicsItem>
#include <QMessageBox>
#include "common/assert.h"
#include "common/file_util.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_system.h"
#include "yuzu/configuration/configure_system.h"

namespace {
constexpr std::array<int, 12> days_in_month = {{
    31,
    29,
    31,
    30,
    31,
    30,
    31,
    31,
    30,
    31,
    30,
    31,
}};
} // Anonymous namespace

ConfigureSystem::ConfigureSystem(QWidget* parent) : QWidget(parent), ui(new Ui::ConfigureSystem) {
    ui->setupUi(this);
    connect(ui->combo_birthmonth,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &ConfigureSystem::UpdateBirthdayComboBox);
    connect(ui->button_regenerate_console_id, &QPushButton::clicked, this,
            &ConfigureSystem::RefreshConsoleID);

    connect(ui->rng_seed_checkbox, &QCheckBox::stateChanged, this, [this](bool checked) {
        ui->rng_seed_edit->setEnabled(checked);
        if (!checked)
            ui->rng_seed_edit->setText(QStringLiteral("00000000"));
    });

    connect(ui->custom_rtc_checkbox, &QCheckBox::stateChanged, this, [this](bool checked) {
        ui->custom_rtc_edit->setEnabled(checked);
        if (!checked)
            ui->custom_rtc_edit->setDateTime(QDateTime::currentDateTime());
    });

    this->setConfiguration();
}

ConfigureSystem::~ConfigureSystem() = default;

void ConfigureSystem::setConfiguration() {
    enabled = !Core::System::GetInstance().IsPoweredOn();

    ui->combo_language->setCurrentIndex(Settings::values.language_index);

    ui->rng_seed_checkbox->setChecked(Settings::values.rng_seed.has_value());
    ui->rng_seed_edit->setEnabled(Settings::values.rng_seed.has_value());

    const auto rng_seed = QStringLiteral("%1")
                              .arg(Settings::values.rng_seed.value_or(0), 8, 16, QLatin1Char{'0'})
                              .toUpper();
    ui->rng_seed_edit->setText(rng_seed);

    ui->custom_rtc_checkbox->setChecked(Settings::values.custom_rtc.has_value());
    ui->custom_rtc_edit->setEnabled(Settings::values.custom_rtc.has_value());

    const auto rtc_time = Settings::values.custom_rtc.value_or(
        std::chrono::seconds(QDateTime::currentSecsSinceEpoch()));
    ui->custom_rtc_edit->setDateTime(QDateTime::fromSecsSinceEpoch(rtc_time.count()));
}

void ConfigureSystem::ReadSystemSettings() {}

void ConfigureSystem::applyConfiguration() {
    if (!enabled)
        return;

    Settings::values.language_index = ui->combo_language->currentIndex();

    if (ui->rng_seed_checkbox->isChecked())
        Settings::values.rng_seed = ui->rng_seed_edit->text().toULongLong(nullptr, 16);
    else
        Settings::values.rng_seed = std::nullopt;

    if (ui->custom_rtc_checkbox->isChecked())
        Settings::values.custom_rtc =
            std::chrono::seconds(ui->custom_rtc_edit->dateTime().toSecsSinceEpoch());
    else
        Settings::values.custom_rtc = std::nullopt;

    Settings::Apply();
}

void ConfigureSystem::UpdateBirthdayComboBox(int birthmonth_index) {
    if (birthmonth_index < 0 || birthmonth_index >= 12)
        return;

    // store current day selection
    int birthday_index = ui->combo_birthday->currentIndex();

    // get number of days in the new selected month
    int days = days_in_month[birthmonth_index];

    // if the selected day is out of range,
    // reset it to 1st
    if (birthday_index < 0 || birthday_index >= days)
        birthday_index = 0;

    // update the day combo box
    ui->combo_birthday->clear();
    for (int i = 1; i <= days; ++i) {
        ui->combo_birthday->addItem(QString::number(i));
    }

    // restore the day selection
    ui->combo_birthday->setCurrentIndex(birthday_index);
}

void ConfigureSystem::RefreshConsoleID() {
    QMessageBox::StandardButton reply;
    QString warning_text = tr("This will replace your current virtual Switch with a new one. "
                              "Your current virtual Switch will not be recoverable. "
                              "This might have unexpected effects in games. This might fail, "
                              "if you use an outdated config savegame. Continue?");
    reply = QMessageBox::critical(this, tr("Warning"), warning_text,
                                  QMessageBox::No | QMessageBox::Yes);
    if (reply == QMessageBox::No)
        return;
    u64 console_id{};
    ui->label_console_id->setText(
        tr("Console ID: 0x%1").arg(QString::number(console_id, 16).toUpper()));
}
