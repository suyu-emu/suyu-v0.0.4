// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <QBoxLayout>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QStyle>
#include <QWidget>
#include <qabstractbutton.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qnamespace.h>
#include <qsizepolicy.h>
#include "common/settings.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_per_game.h"
#include "yuzu/configuration/shared_translation.h"

namespace ConfigurationShared {

static QPushButton* CreateClearGlobalButton(QWidget* parent, Settings::BasicSetting* setting) {
    QStyle* style = parent->style();
    QIcon* icon = new QIcon(style->standardIcon(QStyle::SP_DialogResetButton));
    QPushButton* button = new QPushButton(*icon, QStringLiteral(""), parent);
    button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);

    QSizePolicy sp_retain = button->sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    button->setSizePolicy(sp_retain);

    button->setEnabled(!setting->UsingGlobal());
    button->setVisible(!setting->UsingGlobal());

    return button;
}

static std::pair<QWidget*, std::function<void()>> CreateCheckBox(Settings::BasicSetting* setting,
                                                                 const QString& label,
                                                                 QWidget* parent,
                                                                 std::list<CheckState>& trackers) {
    QWidget* widget = new QWidget(parent);
    QHBoxLayout* layout = new QHBoxLayout(widget);

    QCheckBox* checkbox = new QCheckBox(label, parent);
    checkbox->setObjectName(QString::fromStdString(setting->GetLabel()));
    checkbox->setCheckState(setting->ToString() == "true" ? Qt::CheckState::Checked
                                                          : Qt::CheckState::Unchecked);

    std::function<void()> load_func;

    layout->addWidget(checkbox);
    if (Settings::IsConfiguringGlobal()) {
        load_func = [setting, checkbox]() {
            setting->LoadString(checkbox->checkState() == Qt::Checked ? "true" : "false");
        };
    } else {
        auto* button = CreateClearGlobalButton(parent, setting);
        layout->addWidget(button);

        QObject::connect(checkbox, &QCheckBox::stateChanged, [button](int) {
            button->setVisible(true);
            button->setEnabled(true);
        });

        QObject::connect(button, &QAbstractButton::clicked, [checkbox, setting, button](bool) {
            checkbox->setCheckState(setting->ToStringGlobal() == "true" ? Qt::Checked
                                                                        : Qt::Unchecked);
            button->setEnabled(false);
            button->setVisible(false);
        });

        load_func = [setting, checkbox, button]() {
            bool using_global = !button->isEnabled();
            setting->SetGlobal(using_global);
            if (!using_global) {
                setting->LoadString(checkbox->checkState() == Qt::Checked ? "true" : "false");
            }
        };
    }

    layout->setContentsMargins(0, 0, 0, 0);

    return {widget, load_func};
}

static std::tuple<QWidget*, QComboBox*, QPushButton*, std::function<void()>> CreateCombobox(
    Settings::BasicSetting* setting, const QString& label, QWidget* parent, bool managed) {
    const auto type = setting->TypeId();

    QWidget* group = new QWidget(parent);
    group->setObjectName(QString::fromStdString(setting->GetLabel()));
    QLayout* layout = new QHBoxLayout(group);

    QLabel* qt_label = new QLabel(label, parent);
    QComboBox* combobox = new QComboBox(parent);

    QPushButton* button{nullptr};

    std::forward_list<QString> combobox_enumerations = ComboboxEnumeration(type, parent);
    for (const auto& item : combobox_enumerations) {
        combobox->addItem(item);
    }

    layout->addWidget(qt_label);
    layout->addWidget(combobox);

    layout->setSpacing(6);
    layout->setContentsMargins(0, 0, 0, 0);

    combobox->setCurrentIndex(std::stoi(setting->ToString()));

    std::function<void()> load_func = []() {};

    if (Settings::IsConfiguringGlobal() && managed) {
        load_func = [setting, combobox]() {
            setting->LoadString(std::to_string(combobox->currentIndex()));
        };
    } else if (managed) {
        button = CreateClearGlobalButton(parent, setting);
        layout->addWidget(button);

        QObject::connect(button, &QAbstractButton::clicked, [button, combobox, setting](bool) {
            button->setEnabled(false);
            button->setVisible(false);

            combobox->setCurrentIndex(std::stoi(setting->ToStringGlobal()));
        });

        QObject::connect(combobox, QOverload<int>::of(&QComboBox::activated), [=](int) {
            button->setEnabled(true);
            button->setVisible(true);
        });

        load_func = [setting, combobox, button]() {
            bool using_global = !button->isEnabled();
            setting->SetGlobal(using_global);
            if (!using_global) {
                setting->LoadString(std::to_string(combobox->currentIndex()));
            }
        };
    }

    return {group, combobox, button, load_func};
}

static std::tuple<QWidget*, void*, std::function<void()>> CreateLineEdit(
    Settings::BasicSetting* setting, const QString& label, QWidget* parent) {
    QWidget* widget = new QWidget(parent);
    widget->setObjectName(label);

    QHBoxLayout* layout = new QHBoxLayout(widget);
    QLineEdit* line_edit = new QLineEdit(parent);

    const QString text = QString::fromStdString(setting->ToString());
    line_edit->setText(text);

    std::function<void()> load_func;

    // setSizePolicy lets widget expand and take an equal part of the space as the line edit
    if (Settings::IsConfiguringGlobal()) {
        QLabel* q_label = new QLabel(label, widget);
        q_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(q_label);

        load_func = [line_edit, setting]() {
            std::string load_text = line_edit->text().toStdString();
            setting->LoadString(load_text);
        };
    } else {
        QCheckBox* checkbox = new QCheckBox(label, parent);
        checkbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(checkbox);

        const auto highlight_func = [widget, line_edit](int state) {
            bool using_global = state != Qt::Checked;
            SetHighlight(widget, !using_global);
            line_edit->setEnabled(!using_global);
        };

        QObject::connect(checkbox, qOverload<int>(&QCheckBox::stateChanged), widget,
                         highlight_func);

        checkbox->setCheckState(setting->UsingGlobal() ? Qt::Unchecked : Qt::Checked);
        highlight_func(checkbox->checkState());

        load_func = [checkbox, setting, line_edit]() {
            if (checkbox->checkState() == Qt::Checked) {
                setting->SetGlobal(false);

                std::string load_text = line_edit->text().toStdString();
                setting->LoadString(load_text);
            } else {
                setting->SetGlobal(true);
            }
        };
    }

    layout->addWidget(line_edit);

    layout->setContentsMargins(0, 0, 0, 0);

    return {widget, line_edit, load_func};
}

std::tuple<QWidget*, void*, QPushButton*> CreateWidget(
    Settings::BasicSetting* setting, const TranslationMap& translations, QWidget* parent,
    bool runtime_lock, std::forward_list<std::function<void(bool)>>& apply_funcs,
    std::list<CheckState>& trackers, RequestType request, bool managed) {
    if (!Settings::IsConfiguringGlobal() && !setting->Switchable()) {
        LOG_DEBUG(Frontend, "\"{}\" is not switchable, skipping...", setting->GetLabel());
        return {nullptr, nullptr, nullptr};
    }

    const auto type = setting->TypeId();
    const int id = setting->Id();
    QWidget* widget{nullptr};
    void* extra{nullptr};

    std::function<void()> load_func;

    const auto [label, tooltip] = [&]() {
        const auto& setting_label = setting->GetLabel();
        if (translations.contains(id)) {
            return std::pair{translations.at(id).first, translations.at(id).second};
        }
        LOG_ERROR(Frontend, "Translation table lacks entry for \"{}\"", setting_label);
        return std::pair{QString::fromStdString(setting_label), QStringLiteral("")};
    }();

    if (label == QStringLiteral("")) {
        LOG_DEBUG(Frontend, "Translation table has emtpy entry for \"{}\", skipping...",
                  setting->GetLabel());
        return {nullptr, nullptr, nullptr};
    }

    QPushButton* button;

    if (type == typeid(bool)) {
        auto pair = CreateCheckBox(setting, label, parent, trackers);
        widget = pair.first;
        load_func = pair.second;
    } else if (setting->IsEnum()) {
        auto tuple = CreateCombobox(setting, label, parent, managed);
        widget = std::get<0>(tuple);
        extra = std::get<1>(tuple);
        button = std::get<2>(tuple);
        load_func = std::get<3>(tuple);
    } else if (type == typeid(u32) || type == typeid(int)) {
        switch (request) {
        case RequestType::Default: {
            auto tuple = CreateLineEdit(setting, label, parent);
            widget = std::get<0>(tuple);
            extra = std::get<1>(tuple);
            load_func = std::get<2>(tuple);
            break;
        }
        case RequestType::ComboBox: {
            auto tuple = CreateCombobox(setting, label, parent, managed);
            widget = std::get<0>(tuple);
            extra = std::get<1>(tuple);
            button = std::get<2>(tuple);
            load_func = std::get<3>(tuple);
            break;
        }
        case RequestType::SpinBox:
        case RequestType::Slider:
        case RequestType::MaxEnum:
            break;
        }
    }

    if (widget == nullptr) {
        LOG_ERROR(Frontend, "No widget was created for \"{}\"", setting->GetLabel());
        return {nullptr, nullptr, nullptr};
    }

    apply_funcs.push_front([load_func, setting](bool powered_on) {
        if (setting->RuntimeModfiable() || !powered_on) {
            load_func();
        }
    });

    bool enable = runtime_lock || setting->RuntimeModfiable();
    if (setting->Switchable() && Settings::IsConfiguringGlobal() && !runtime_lock) {
        enable &= !setting->UsingGlobal();
    }
    widget->setEnabled(enable);

    widget->setVisible(Settings::IsConfiguringGlobal() || setting->Switchable());

    widget->setToolTip(tooltip);

    return {widget, extra, button};
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
        widget->setStyleSheet(QStringLiteral(""));
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
