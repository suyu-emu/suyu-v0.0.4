// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QString>
#include <QWidget>
#include <qnamespace.h>
#include "common/settings.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_per_game.h"
#include "yuzu/configuration/shared_translation.h"

namespace ConfigurationShared {
static std::pair<QWidget*, std::function<void()>> CreateCheckBox(Settings::BasicSetting* setting,
                                                                 const QString& label,
                                                                 QWidget* parent,
                                                                 std::list<CheckState>& trackers) {
    QCheckBox* checkbox = new QCheckBox(label, parent);
    checkbox->setObjectName(QString::fromStdString(setting->GetLabel()));
    checkbox->setCheckState(setting->ToString() == "true" ? Qt::CheckState::Checked
                                                          : Qt::CheckState::Unchecked);

    CheckState* tracker{};

    // Per-game config highlight
    if (setting->Switchable() && !Settings::IsConfiguringGlobal()) {
        bool global_state = setting->ToStringGlobal() == "true";
        bool state = setting->ToString() == "true";
        bool global = setting->UsingGlobal();
        tracker = &trackers.emplace_front(CheckState{});
        SetColoredTristate(checkbox, global, state, global_state, *tracker);
    }

    auto load_func = [checkbox, setting, tracker]() {
        if (Settings::IsConfiguringGlobal()) {
            setting->LoadString(checkbox->checkState() == Qt::Checked ? "true" : "false");
        }

        if (Settings::IsConfiguringGlobal() || !setting->Switchable()) {
            return;
        }

        if (*tracker != CheckState::Global) {
            setting->SetGlobal(false);
            setting->LoadString(checkbox->checkState() == Qt::Checked ? "true" : "false");
        } else {
            setting->SetGlobal(true);
        }
    };

    return {checkbox, load_func};
}

static std::pair<QWidget*, std::function<void()>> CreateCombobox(Settings::BasicSetting* setting,
                                                                 const QString& label,
                                                                 QWidget* parent) {
    const auto type = setting->TypeId();

    QWidget* group = new QWidget(parent);
    group->setObjectName(QString::fromStdString(setting->GetLabel()));
    QLayout* combobox_layout = new QHBoxLayout(group);

    QLabel* qt_label = new QLabel(label, parent);
    QComboBox* combobox = new QComboBox(parent);

    std::forward_list<QString> combobox_enumerations = ComboboxEnumeration(type, parent);
    for (const auto& item : combobox_enumerations) {
        combobox->addItem(item);
    }

    combobox_layout->addWidget(qt_label);
    combobox_layout->addWidget(combobox);

    combobox_layout->setSpacing(6);
    combobox_layout->setContentsMargins(0, 0, 0, 0);

    if (setting->Switchable() && !Settings::IsConfiguringGlobal()) {
        int current = std::stoi(setting->ToString());
        int global_value = std::stoi(setting->ToStringGlobal());
        SetColoredComboBox(combobox, group, global_value);
        if (setting->UsingGlobal()) {
            combobox->setCurrentIndex(USE_GLOBAL_INDEX);
        } else {
            SetHighlight(group, true);
            combobox->setCurrentIndex(current + USE_GLOBAL_OFFSET);
        }
    } else {
        combobox->setCurrentIndex(std::stoi(setting->ToString()));
    }

    const auto load_func = [combobox, setting]() {
        if (Settings::IsConfiguringGlobal()) {
            setting->LoadString(std::to_string(combobox->currentIndex()));
        }

        if (Settings::IsConfiguringGlobal() || !setting->Switchable()) {
            return;
        }

        bool using_global = combobox->currentIndex() == USE_GLOBAL_INDEX;
        int index = combobox->currentIndex() - USE_GLOBAL_OFFSET;

        setting->SetGlobal(using_global);
        if (!using_global) {
            setting->LoadString(std::to_string(index));
        }
    };

    return {group, load_func};
}

QWidget* CreateWidget(Settings::BasicSetting* setting, const TranslationMap& translations,
                      QWidget* parent, bool runtime_lock,
                      std::forward_list<std::function<void(bool)>>& apply_funcs,
                      std::list<CheckState>& trackers) {
    const auto type = setting->TypeId();
    QWidget* widget{nullptr};

    std::function<void()> load_func;

    const auto [label, tooltip] = [&]() {
        const auto& setting_label = setting->GetLabel();
        if (translations.contains(setting_label)) {
            return std::pair{translations.at(setting_label).first,
                             translations.at(setting_label).second};
        }
        LOG_ERROR(Frontend, "Translation table lacks entry for \"{}\"", setting_label);
        return std::pair{QString::fromStdString(setting_label), QStringLiteral("")};
    }();

    if (label == QStringLiteral("")) {
        LOG_DEBUG(Frontend, "Translation table has emtpy entry for \"{}\", skipping...",
                  setting->GetLabel());
        return widget;
    }

    if (type == typeid(bool)) {
        auto pair = CreateCheckBox(setting, label, parent, trackers);
        widget = pair.first;
        load_func = pair.second;
    } else if (setting->IsEnum()) {
        auto pair = CreateCombobox(setting, label, parent);
        widget = pair.first;
        load_func = pair.second;
    }

    if (widget == nullptr) {
        LOG_ERROR(Frontend, "No widget was created for \"{}\"", setting->GetLabel());
        return widget;
    }

    apply_funcs.push_front([load_func, setting](bool powered_on) {
        if (setting->RuntimeModfiable() || !powered_on) {
            load_func();
        }
    });

    bool enable = runtime_lock || setting->RuntimeModfiable();
    enable &=
        setting->Switchable() && !(Settings::IsConfiguringGlobal() && !setting->UsingGlobal());

    widget->setEnabled(enable);
    widget->setVisible(Settings::IsConfiguringGlobal() || setting->Switchable());

    widget->setToolTip(tooltip);

    return widget;
}

Tab::Tab(std::shared_ptr<std::forward_list<Tab*>> group_, QWidget* parent)
    : QWidget(parent), group{group_} {
    if (group != nullptr) {
        group->push_front(this);
    }
}

Tab::~Tab() = default;

} // namespace ConfigurationShared

void ConfigurationShared::ApplyPerGameSetting(Settings::SwitchableSetting<bool>* setting,
                                              const QCheckBox* checkbox,
                                              const CheckState& tracker) {
    if (Settings::IsConfiguringGlobal() && setting->UsingGlobal()) {
        setting->SetValue(checkbox->checkState());
    } else if (!Settings::IsConfiguringGlobal()) {
        if (tracker == CheckState::Global) {
            setting->SetGlobal(true);
        } else {
            setting->SetGlobal(false);
            setting->SetValue(checkbox->checkState());
        }
    }
}

void ConfigurationShared::SetPerGameSetting(QCheckBox* checkbox,
                                            const Settings::SwitchableSetting<bool>* setting) {
    if (setting->UsingGlobal()) {
        checkbox->setCheckState(Qt::PartiallyChecked);
    } else {
        checkbox->setCheckState(setting->GetValue() ? Qt::Checked : Qt::Unchecked);
    }
}

void ConfigurationShared::SetHighlight(QWidget* widget, bool highlighted) {
    if (highlighted) {
        widget->setStyleSheet(QStringLiteral("QWidget#%1 { background-color:rgba(0,203,255,0.5) }")
                                  .arg(widget->objectName()));
    } else {
        widget->setStyleSheet(QStringLiteral("QWidget#%1 { background-color:rgba(0,0,0,0) }")
                                  .arg(widget->objectName()));
    }
    widget->show();
}

void ConfigurationShared::SetColoredTristate(QCheckBox* checkbox, bool global, bool state,
                                             bool global_state, CheckState& tracker) {
    if (global) {
        tracker = CheckState::Global;
    } else {
        tracker = (state == global_state) ? CheckState::On : CheckState::Off;
    }
    SetHighlight(checkbox, tracker != CheckState::Global);
    QObject::connect(checkbox, &QCheckBox::clicked, checkbox, [checkbox, global_state, &tracker] {
        tracker = static_cast<CheckState>((static_cast<int>(tracker) + 1) %
                                          static_cast<int>(CheckState::Count));
        if (tracker == CheckState::Global) {
            checkbox->setChecked(global_state);
        }
        SetHighlight(checkbox, tracker != CheckState::Global);
    });
}

void ConfigurationShared::SetColoredComboBox(QComboBox* combobox, QWidget* target, int global) {
    InsertGlobalItem(combobox, global);
    QObject::connect(combobox, qOverload<int>(&QComboBox::activated), target,
                     [target](int index) { SetHighlight(target, index != 0); });
}

void ConfigurationShared::InsertGlobalItem(QComboBox* combobox, int global_index) {
    const QString use_global_text =
        ConfigurePerGame::tr("Use global configuration (%1)").arg(combobox->itemText(global_index));
    combobox->insertItem(ConfigurationShared::USE_GLOBAL_INDEX, use_global_text);
    combobox->insertSeparator(ConfigurationShared::USE_GLOBAL_SEPARATOR_INDEX);
}

int ConfigurationShared::GetComboboxIndex(int global_setting_index, const QComboBox* combobox) {
    if (Settings::IsConfiguringGlobal()) {
        return combobox->currentIndex();
    }
    if (combobox->currentIndex() == ConfigurationShared::USE_GLOBAL_INDEX) {
        return global_setting_index;
    }
    return combobox->currentIndex() - ConfigurationShared::USE_GLOBAL_OFFSET;
}
