// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>
#include <forward_list>
#include <optional>

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QFileDialog>
#include <QGraphicsItem>
#include <QLineEdit>
#include <QMessageBox>
#include "common/settings.h"
#include "core/core.h"
#include "core/hle/service/time/time_manager.h"
#include "ui_configure_system.h"
#include "yuzu/configuration/config.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_system.h"
#include "yuzu/configuration/shared_widget.h"

constexpr std::array<u32, 7> LOCALE_BLOCKLIST{
    // pzzefezrpnkzeidfej
    // thhsrnhutlohsternp
    // BHH4CG          U
    // Raa1AB          S
    //  nn9
    //  ts
    0b0100011100001100000, // Japan
    0b0000001101001100100, // Americas
    0b0100110100001000010, // Europe
    0b0100110100001000010, // Australia
    0b0000000000000000000, // China
    0b0100111100001000000, // Korea
    0b0100111100001000000, // Taiwan
};

static bool IsValidLocale(u32 region_index, u32 language_index) {
    if (region_index >= LOCALE_BLOCKLIST.size()) {
        return false;
    }
    return ((LOCALE_BLOCKLIST.at(region_index) >> language_index) & 1) == 0;
}

ConfigureSystem::ConfigureSystem(
    Core::System& system_, std::shared_ptr<std::forward_list<ConfigurationShared::Tab*>> group_,
    const ConfigurationShared::TranslationMap& translations_,
    const ConfigurationShared::ComboboxTranslationMap& combobox_translations_, QWidget* parent)
    : Tab(group_, parent), ui{std::make_unique<Ui::ConfigureSystem>()}, system{system_},
      translations{translations_}, combobox_translations{combobox_translations_} {
    ui->setupUi(this);

    Setup();

    connect(rng_seed_checkbox, &QCheckBox::stateChanged, this, [this](int state) {
        rng_seed_edit->setEnabled(state == Qt::Checked);
        if (state != Qt::Checked) {
            rng_seed_edit->setText(QStringLiteral("00000000"));
        }
    });

    connect(custom_rtc_checkbox, &QCheckBox::stateChanged, this, [this](int state) {
        custom_rtc_edit->setEnabled(state == Qt::Checked);
        if (state != Qt::Checked) {
            custom_rtc_edit->setDateTime(QDateTime::currentDateTime());
        }
    });

    const auto locale_check = [this]() {
        const auto region_index = combo_region->currentIndex();
        const auto language_index = combo_language->currentIndex();
        const bool valid_locale = IsValidLocale(region_index, language_index);
        ui->label_warn_invalid_locale->setVisible(!valid_locale);
        if (!valid_locale) {
            ui->label_warn_invalid_locale->setText(
                tr("Warning: \"%1\" is not a valid language for region \"%2\"")
                    .arg(combo_language->currentText())
                    .arg(combo_region->currentText()));
        }
    };

    connect(combo_language, qOverload<int>(&QComboBox::currentIndexChanged), this, locale_check);
    connect(combo_region, qOverload<int>(&QComboBox::currentIndexChanged), this, locale_check);

    ui->label_warn_invalid_locale->setVisible(false);
    locale_check();

    SetConfiguration();
}

ConfigureSystem::~ConfigureSystem() = default;

void ConfigureSystem::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureSystem::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureSystem::Setup() {
    const bool runtime_lock = !system.IsPoweredOn();
    auto& core_layout = *ui->core_widget->layout();
    auto& system_layout = *ui->system_widget->layout();

    std::map<u32, QWidget*> core_hold{};
    std::map<u32, QWidget*> system_hold{};

    std::forward_list<Settings::BasicSetting*> settings;
    auto push = [&settings](std::forward_list<Settings::BasicSetting*>& list) {
        for (auto setting : list) {
            settings.push_front(setting);
        }
    };

    push(Settings::values.linkage.by_category[Settings::Category::Core]);
    push(Settings::values.linkage.by_category[Settings::Category::System]);

    for (auto setting : settings) {
        if (!Settings::IsConfiguringGlobal() && !setting->Switchable()) {
            continue;
        }

        [[maybe_unused]] std::string label = setting->GetLabel();
        ConfigurationShared::Widget* widget = [this, setting, runtime_lock]() {
            if (setting->Id() == Settings::values.custom_rtc.Id()) {
                // custom_rtc needs a DateTimeEdit (default is LineEdit), and a checkbox to manage
                // it and custom_rtc_enabled
                return new ConfigurationShared::Widget(
                    setting, translations, combobox_translations, this, runtime_lock, apply_funcs,
                    &Settings::values.custom_rtc_enabled,
                    ConfigurationShared::RequestType::DateTimeEdit);
            } else if (setting->Id() == Settings::values.rng_seed.Id()) {
                // rng_seed needs a HexEdit (default is LineEdit), and a checkbox to manage
                // it and rng_seed_enabled
                return new ConfigurationShared::Widget(
                    setting, translations, combobox_translations, this, runtime_lock, apply_funcs,
                    &Settings::values.rng_seed_enabled, ConfigurationShared::RequestType::HexEdit);
            } else {
                return new ConfigurationShared::Widget(setting, translations, combobox_translations,
                                                       this, runtime_lock, apply_funcs);
            }
        }();

        if (!widget->Valid()) {
            delete widget;
            continue;
        }

        if (setting->Id() == Settings::values.rng_seed.Id()) {
            // Keep track of rng_seed's widgets to reset it with the checkbox state
            rng_seed_checkbox = widget->checkbox;
            rng_seed_edit = widget->line_edit;

            rng_seed_edit->setEnabled(Settings::values.rng_seed_enabled.GetValue());
        } else if (setting->Id() == Settings::values.custom_rtc.Id()) {
            // Keep track of custom_rtc's widgets to reset it with the checkbox state
            custom_rtc_checkbox = widget->checkbox;
            custom_rtc_edit = widget->date_time_edit;

            custom_rtc_edit->setEnabled(Settings::values.custom_rtc_enabled.GetValue());
        } else if (setting->Id() == Settings::values.region_index.Id()) {
            // Keep track of the region_index (and langauge_index) combobox to validate the selected
            // settings
            combo_region = widget->combobox;
        } else if (setting->Id() == Settings::values.language_index.Id()) {
            combo_language = widget->combobox;
        }

        switch (setting->Category()) {
        case Settings::Category::Core:
            core_hold.emplace(setting->Id(), widget);
            break;
        case Settings::Category::System:
            system_hold.emplace(setting->Id(), widget);
            break;
        default:
            delete widget;
        }
    }
    for (const auto& [label, widget] : core_hold) {
        core_layout.addWidget(widget);
    }
    for (const auto& [id, widget] : system_hold) {
        system_layout.addWidget(widget);
    }
}

void ConfigureSystem::SetConfiguration() {}

void ConfigureSystem::ApplyConfiguration() {
    const bool powered_on = system.IsPoweredOn();
    for (const auto& func : apply_funcs) {
        func(powered_on);
    }
}
