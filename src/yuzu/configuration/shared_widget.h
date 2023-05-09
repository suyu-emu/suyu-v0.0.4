#pragma once

#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/shared_translation.h"

class QPushButton;
class QSpinBox;
class QComboBox;
class QLineEdit;
class QSlider;
class QCheckBox;

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
    MaxEnum,
};

class Widget : public QWidget {
    Q_OBJECT

public:
    Widget(Settings::BasicSetting* setting, const TranslationMap& translations, QWidget* parent,
           bool runtime_lock, std::forward_list<std::function<void(bool)>>& apply_funcs,
           RequestType request = RequestType::Default, bool managed = true, float multiplier = 1.0f,
           Settings::BasicSetting* other_setting = nullptr,
           const QString& format = QStringLiteral(""));
    virtual ~Widget();

    bool Valid();

    [[nodiscard]] static QPushButton* CreateRestoreGlobalButton(Settings::BasicSetting& setting,
                                                                QWidget* parent);

    QPushButton* restore_button{};
    QLineEdit* line_edit{};
    QSpinBox* spinbox{};
    QCheckBox* checkbox{};
    QSlider* slider{};
    QComboBox* combobox{};

private:
    void CreateCheckBox(const QString& label, std::function<void()>& load_func);
    void CreateCheckBoxWithLineEdit(const QString& label, Settings::BasicSetting* other_setting,
                                    std::function<void()>& load_func);
    void CreateCheckBoxWithSpinBox(const QString& label, Settings::BasicSetting* other_setting,
                                   std::function<void()>& load_func, const QString& suffix);
    void CreateCombobox(const QString& label, bool managed, std::function<void()>& load_func);
    void CreateLineEdit(const QString& label, bool managed, std::function<void()>& load_func);
    void CreateSlider(const QString& label, bool reversed, float multiplier,
                      std::function<void()>& load_func);

    QWidget* parent;
    const TranslationMap& translations;
    Settings::BasicSetting& setting;

    bool created{false};
};

} // namespace ConfigurationShared
