// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/shared_translation.h"

class QPushButton;
class QSpinBox;
class QComboBox;
class QLineEdit;
class QSlider;
class QCheckBox;
class QLabel;
class QHBoxLayout;
class QDateTimeEdit;

namespace Settings {
class BasicSetting;
}

namespace ConfigurationShared {

enum class RequestType {
    Default,
    ComboBox,
    SpinBox,
    Slider,
    ReverseSlider,
    LineEdit,
    HexEdit,
    DateTimeEdit,
    MaxEnum,
};

class Widget : public QWidget {
    Q_OBJECT

public:
    Widget(Settings::BasicSetting* setting, const TranslationMap& translations,
           const ComboboxTranslationMap& combobox_translations, QWidget* parent, bool runtime_lock,
           std::forward_list<std::function<void(bool)>>& apply_funcs_,
           RequestType request = RequestType::Default, bool managed = true, float multiplier = 1.0f,
           Settings::BasicSetting* other_setting = nullptr,
           const QString& string = QStringLiteral(""));
    virtual ~Widget();

    bool Valid() const;

    [[nodiscard]] static QPushButton* CreateRestoreGlobalButton(bool using_global, QWidget* parent);

    QPushButton* restore_button{};
    QLineEdit* line_edit{};
    QSpinBox* spinbox{};
    QCheckBox* checkbox{};
    QSlider* slider{};
    QComboBox* combobox{};
    QDateTimeEdit* date_time_edit{};

private:
    void SetupComponent(const QString& label, std::function<void()>& load_func, bool managed,
                        RequestType request, float multiplier,
                        Settings::BasicSetting* other_setting, const QString& string);

    QLabel* CreateLabel(const QString& text);
    QWidget* CreateCheckBox(Settings::BasicSetting* bool_setting, const QString& label,
                            std::function<std::string()>& serializer,
                            std::function<void()>& restore_func,
                            const std::function<void()>& touch);

    QWidget* CreateCombobox(std::function<std::string()>& serializer,
                            std::function<void()>& restore_func,
                            const std::function<void()>& touch);
    QWidget* CreateLineEdit(std::function<std::string()>& serializer,
                            std::function<void()>& restore_func, const std::function<void()>& touch,
                            bool managed = true);
    QWidget* CreateHexEdit(std::function<std::string()>& serializer,
                           std::function<void()>& restore_func, const std::function<void()>& touch);
    QWidget* CreateSlider(bool reversed, float multiplier, const QString& format,
                          std::function<std::string()>& serializer,
                          std::function<void()>& restore_func, const std::function<void()>& touch);
    QWidget* CreateDateTimeEdit(bool disabled, bool restrict,
                                std::function<std::string()>& serializer,
                                std::function<void()>& restore_func,
                                const std::function<void()>& touch);
    QWidget* CreateSpinBox(const QString& suffix, std::function<std::string()>& serializer,
                           std::function<void()>& restore_func, const std::function<void()>& touch);

    QWidget* parent;
    const TranslationMap& translations;
    const ComboboxTranslationMap& combobox_enumerations;
    Settings::BasicSetting& setting;
    std::forward_list<std::function<void(bool)>>& apply_funcs;

    bool created{false};
    bool runtime_lock{false};
};

} // namespace ConfigurationShared
