// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>
#include <optional>
#include <vector>

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

ConfigureSystem::ConfigureSystem(Core::System& system_,
                                 std::shared_ptr<std::vector<ConfigurationShared::Tab*>> group_,
                                 const ConfigurationShared::Builder& builder, QWidget* parent)
    : Tab(group_, parent), ui{std::make_unique<Ui::ConfigureSystem>()}, system{system_} {
    ui->setupUi(this);

    Setup(builder);

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

void ConfigureSystem::Setup(const ConfigurationShared::Builder& builder) {
    auto& core_layout = *ui->core_widget->layout();
    auto& system_layout = *ui->system_widget->layout();

    std::map<u32, QWidget*> core_hold{};
    std::map<u32, QWidget*> system_hold{};

    std::vector<Settings::BasicSetting*> settings;
    auto push = [&settings](auto& list) {
        for (auto setting : list) {
            settings.push_back(setting);
        }
    };

    push(Settings::values.linkage.by_category[Settings::Category::Core]);
    push(Settings::values.linkage.by_category[Settings::Category::System]);

    for (auto setting : settings) {
        if (setting->Id() == Settings::values.use_docked_mode.Id() &&
            Settings::IsConfiguringGlobal()) {
            continue;
        }

        ConfigurationShared::Widget* widget = builder.BuildWidget(setting, apply_funcs);

        if (widget == nullptr) {
            continue;
        }
        if (!widget->Valid()) {
            widget->deleteLater();
            continue;
        }

        if (setting->Id() == Settings::values.region_index.Id()) {
            // Keep track of the region_index (and language_index) combobox to validate the selected
            // settings
            combo_region = widget->combobox;
        } else if (setting->Id() == Settings::values.language_index.Id()) {
            combo_language = widget->combobox;
        }

        switch (setting->GetCategory()) {
        case Settings::Category::Core:
            core_hold.emplace(setting->Id(), widget);
            break;
        case Settings::Category::System:
            system_hold.emplace(setting->Id(), widget);
            break;
        default:
            widget->deleteLater();
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
