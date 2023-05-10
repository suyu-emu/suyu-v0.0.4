#include <functional>
#include <limits>
#include <QCheckBox>
#include <QDateTimeEdit>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QWidget>
#include <qabstractbutton.h>
#include <qabstractspinbox.h>
#include <qnamespace.h>
#include <qvalidator.h>
#include "common/common_types.h"
#include "common/settings.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/shared_translation.h"
#include "yuzu/configuration/shared_widget.h"

namespace ConfigurationShared {

static bool IsInt(const std::type_index& type) {
    return type == typeid(u32) || type == typeid(s32) || type == typeid(u16) || type == typeid(s16);
}

QPushButton* Widget::CreateRestoreGlobalButton(Settings::BasicSetting& setting, QWidget* parent) {
    QStyle* style = parent->style();
    QIcon* icon = new QIcon(style->standardIcon(QStyle::SP_DialogResetButton));
    QPushButton* restore_button = new QPushButton(*icon, QStringLiteral(""), parent);
    restore_button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    QSizePolicy sp_retain = restore_button->sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    restore_button->setSizePolicy(sp_retain);

    restore_button->setEnabled(!setting.UsingGlobal());
    restore_button->setVisible(!setting.UsingGlobal());

    return restore_button;
}

void Widget::CreateCheckBox(const QString& label, std::function<void()>& load_func) {
    created = true;

    QHBoxLayout* layout = new QHBoxLayout(this);

    checkbox = new QCheckBox(label, this);
    checkbox->setObjectName(QString::fromStdString(setting.GetLabel()));
    checkbox->setCheckState(setting.ToString() == "true" ? Qt::CheckState::Checked
                                                         : Qt::CheckState::Unchecked);

    layout->addWidget(checkbox);
    if (Settings::IsConfiguringGlobal()) {
        load_func = [=]() {
            setting.LoadString(checkbox->checkState() == Qt::Checked ? "true" : "false");
        };
    } else {
        restore_button = CreateRestoreGlobalButton(setting, this);
        layout->addWidget(restore_button);

        QObject::connect(checkbox, &QCheckBox::stateChanged, [&](int) {
            restore_button->setVisible(true);
            restore_button->setEnabled(true);
        });

        QObject::connect(restore_button, &QAbstractButton::clicked, [&](bool) {
            checkbox->setCheckState(setting.ToStringGlobal() == "true" ? Qt::Checked
                                                                       : Qt::Unchecked);
            restore_button->setEnabled(false);
            restore_button->setVisible(false);
        });

        load_func = [=]() {
            bool using_global = !restore_button->isEnabled();
            setting.SetGlobal(using_global);
            if (!using_global) {
                setting.LoadString(checkbox->checkState() == Qt::Checked ? "true" : "false");
            }
        };
    }

    layout->setContentsMargins(0, 0, 0, 0);
}

void Widget::CreateCombobox(const QString& label, bool managed, std::function<void()>& load_func) {
    created = true;

    const auto type = setting.TypeId();

    QLayout* layout = new QHBoxLayout(this);

    QLabel* qt_label = new QLabel(label, this);
    combobox = new QComboBox(this);

    std::forward_list<QString> combobox_enumerations = ComboboxEnumeration(type, this);
    for (const auto& item : combobox_enumerations) {
        combobox->addItem(item);
    }

    layout->addWidget(qt_label);
    layout->addWidget(combobox);

    layout->setSpacing(6);
    layout->setContentsMargins(0, 0, 0, 0);

    combobox->setCurrentIndex(std::stoi(setting.ToString()));

    if (Settings::IsConfiguringGlobal() && managed) {
        load_func = [=]() { setting.LoadString(std::to_string(combobox->currentIndex())); };
    } else if (managed) {
        restore_button = CreateRestoreGlobalButton(setting, this);
        layout->addWidget(restore_button);

        QObject::connect(restore_button, &QAbstractButton::clicked, [&](bool) {
            restore_button->setEnabled(false);
            restore_button->setVisible(false);

            combobox->setCurrentIndex(std::stoi(setting.ToStringGlobal()));
        });

        QObject::connect(combobox, QOverload<int>::of(&QComboBox::activated), [=](int) {
            restore_button->setEnabled(true);
            restore_button->setVisible(true);
        });

        load_func = [=]() {
            bool using_global = !restore_button->isEnabled();
            setting.SetGlobal(using_global);
            if (!using_global) {
                setting.LoadString(std::to_string(combobox->currentIndex()));
            }
        };
    }
}

void Widget::CreateLineEdit(const QString& label, bool managed, std::function<void()>& load_func) {
    created = true;

    QHBoxLayout* layout = new QHBoxLayout(this);
    line_edit = new QLineEdit(this);

    const QString text = QString::fromStdString(setting.ToString());
    line_edit->setText(text);

    QLabel* q_label = new QLabel(label, this);
    // setSizePolicy lets widget expand and take an equal part of the space as the line edit
    q_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(q_label);

    layout->addWidget(line_edit);

    if (Settings::IsConfiguringGlobal() && !managed) {
        load_func = [=]() {
            std::string load_text = line_edit->text().toStdString();
            setting.LoadString(load_text);
        };
    } else if (!managed) {
        restore_button = CreateRestoreGlobalButton(setting, this);
        layout->addWidget(restore_button);

        QObject::connect(restore_button, &QAbstractButton::clicked, [&](bool) {
            restore_button->setEnabled(false);
            restore_button->setVisible(false);

            line_edit->setText(QString::fromStdString(setting.ToStringGlobal()));
        });

        QObject::connect(line_edit, &QLineEdit::textChanged, [&](QString) {
            restore_button->setEnabled(true);
            restore_button->setVisible(true);
        });

        load_func = [=]() {
            bool using_global = !restore_button->isEnabled();
            setting.SetGlobal(using_global);
            if (!using_global) {
                setting.LoadString(line_edit->text().toStdString());
            }
        };
    }

    layout->setContentsMargins(0, 0, 0, 0);
}

void Widget::CreateSlider(const QString& name, bool reversed, float multiplier,
                          std::function<void()>& load_func) {
    created = true;

    QHBoxLayout* layout = new QHBoxLayout(this);
    slider = new QSlider(Qt::Horizontal, this);
    QLabel* label = new QLabel(name, this);
    QLabel* feedback = new QLabel(this);

    layout->addWidget(label);
    layout->addWidget(slider);
    layout->addWidget(feedback);

    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->setContentsMargins(0, 0, 0, 0);

    int max_val = std::stoi(setting.MaxVal());

    QObject::connect(slider, &QAbstractSlider::valueChanged, [=](int value) {
        int present = (reversed ? max_val - value : value) * multiplier;
        feedback->setText(
            QStringLiteral("%1%").arg(QString::fromStdString(std::to_string(present))));
    });

    slider->setValue(std::stoi(setting.ToString()));
    slider->setMinimum(std::stoi(setting.MinVal()));
    slider->setMaximum(max_val);

    if (reversed) {
        slider->setInvertedAppearance(true);
    }

    if (Settings::IsConfiguringGlobal()) {
        load_func = [=]() { setting.LoadString(std::to_string(slider->value())); };
    } else {
        restore_button = CreateRestoreGlobalButton(setting, this);
        layout->addWidget(restore_button);

        QObject::connect(restore_button, &QAbstractButton::clicked, [=](bool) {
            slider->setValue(std::stoi(setting.ToStringGlobal()));

            restore_button->setEnabled(false);
            restore_button->setVisible(false);
        });

        QObject::connect(slider, &QAbstractSlider::sliderMoved, [=](int) {
            restore_button->setEnabled(true);
            restore_button->setVisible(true);
        });

        load_func = [=]() {
            bool using_global = !restore_button->isEnabled();
            setting.SetGlobal(using_global);
            if (!using_global) {
                setting.LoadString(std::to_string(slider->value()));
            }
        };
    }
}

void Widget::CreateCheckBoxWithHexEdit(const QString& label, Settings::BasicSetting* other_setting,
                                       std::function<void()>& load_func) {
    if (other_setting == nullptr) {
        LOG_WARNING(Frontend, "Extra setting is null or not an integer");
        return;
    }
    created = true;

    std::function<void()> checkbox_load_func;
    CreateCheckBox(label, checkbox_load_func);

    auto to_hex = [=](const std::string& input) {
        return QString::fromStdString(fmt::format("{:08x}", std::stoi(input)));
    };

    QHBoxLayout* layout = reinterpret_cast<QHBoxLayout*>(this->layout());
    const QString default_val = to_hex(other_setting->ToString());

    line_edit = new QLineEdit(this);
    line_edit->setText(default_val);

    checkbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->insertWidget(1, line_edit);

    line_edit->setMaxLength(8);
    QRegExpValidator* regex =
        new QRegExpValidator{QRegExp{QStringLiteral("^[0-9a-fA-F]{0,8}$")}, line_edit};
    line_edit->setValidator(regex);

    auto hex_to_dec = [=]() -> std::string {
        return std::to_string(std::stoul(line_edit->text().toStdString(), nullptr, 16));
    };

    if (Settings::IsConfiguringGlobal()) {
        load_func = [=]() {
            checkbox_load_func();
            other_setting->LoadString(hex_to_dec());
        };
    } else {
        QObject::connect(restore_button, &QAbstractButton::clicked, [=](bool) {
            line_edit->setText(to_hex(other_setting->ToStringGlobal()));
        });

        QObject::connect(line_edit, &QLineEdit::textEdited, [=](const QString&) {
            restore_button->setEnabled(true);
            restore_button->setVisible(true);
        });

        load_func = [=]() {
            checkbox_load_func();

            const bool using_global = !restore_button->isEnabled();
            other_setting->SetGlobal(using_global);
            if (!using_global) {
                other_setting->LoadString(hex_to_dec());
            }
        };
    }
}

void Widget::CreateCheckBoxWithLineEdit(const QString& label, Settings::BasicSetting* other_setting,
                                        std::function<void()>& load_func) {
    if (other_setting == nullptr) {
        LOG_WARNING(Frontend, "Extra setting is null or not an integer");
        return;
    }
    created = true;

    std::function<void()> checkbox_load_func;
    CreateCheckBox(label, checkbox_load_func);

    QHBoxLayout* layout = reinterpret_cast<QHBoxLayout*>(this->layout());
    const QString default_val = QString::fromStdString(other_setting->ToString());

    line_edit = new QLineEdit(this);
    line_edit->setText(default_val);

    checkbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->insertWidget(1, line_edit);

    if (Settings::IsConfiguringGlobal()) {
        load_func = [=]() {
            checkbox_load_func();
            other_setting->LoadString(line_edit->text().toStdString());
        };
    } else {
        QObject::connect(restore_button, &QAbstractButton::clicked, [=](bool) {
            line_edit->setText(QString::fromStdString(other_setting->ToStringGlobal()));
        });

        QObject::connect(line_edit, &QLineEdit::textEdited, [=](const QString&) {
            restore_button->setEnabled(true);
            restore_button->setVisible(true);
        });

        load_func = [=]() {
            checkbox_load_func();

            const bool using_global = !restore_button->isEnabled();
            other_setting->SetGlobal(using_global);
            if (!using_global) {
                other_setting->LoadString(line_edit->text().toStdString());
            }
        };
    }
}

void Widget::CreateCheckBoxWithSpinBox(const QString& label, Settings::BasicSetting* other_setting,
                                       std::function<void()>& load_func,
                                       const std::string& suffix) {
    if (other_setting == nullptr && IsInt(other_setting->TypeId())) {
        LOG_WARNING(Frontend, "Extra setting is null or not an integer");
        return;
    }
    created = true;

    std::function<void()> checkbox_load_func;
    CreateCheckBox(label, checkbox_load_func);
    checkbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QHBoxLayout* layout = reinterpret_cast<QHBoxLayout*>(this->layout());

    spinbox = new QSpinBox(this);
    const int min_val = std::stoi(other_setting->MinVal());
    const int max_val = std::stoi(other_setting->MaxVal());
    const int default_val = std::stoi(other_setting->ToString());
    spinbox->setRange(min_val, max_val);
    spinbox->setValue(default_val);
    spinbox->setSuffix(QString::fromStdString(suffix));
    spinbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->insertWidget(1, spinbox);

    if (Settings::IsConfiguringGlobal()) {
        load_func = [=]() {
            checkbox_load_func();
            other_setting->LoadString(std::to_string(spinbox->value()));
        };
    } else {
        QObject::connect(restore_button, &QAbstractButton::clicked, [this, other_setting](bool) {
            spinbox->setValue(std::stoi(other_setting->ToStringGlobal()));
        });

        QObject::connect(spinbox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int) {
            restore_button->setEnabled(true);
            restore_button->setVisible(true);
        });

        load_func = [=]() {
            checkbox_load_func();

            const bool using_global = !restore_button->isEnabled();
            other_setting->SetGlobal(using_global);
            if (!using_global) {
                other_setting->LoadString(std::to_string(spinbox->value()));
            }
        };
    }
}

// Currently tailored to custom_rtc
void Widget::CreateCheckBoxWithDateTimeEdit(const QString& label,
                                            Settings::BasicSetting* other_setting,
                                            std::function<void()>& load_func) {
    if (other_setting == nullptr) {
        LOG_WARNING(Frontend, "Extra setting is null or not an integer");
        return;
    }
    created = true;

    std::function<void()> checkbox_load_func;
    CreateCheckBox(label, checkbox_load_func);

    QHBoxLayout* layout = reinterpret_cast<QHBoxLayout*>(this->layout());
    const bool disabled = setting.ToString() != "true";
    const long long current_time = QDateTime::currentSecsSinceEpoch();
    const s64 the_time = disabled ? current_time : std::stoll(other_setting->ToString());
    const auto default_val = QDateTime::fromSecsSinceEpoch(the_time);

    date_time_edit = new QDateTimeEdit(this);
    date_time_edit->setDateTime(default_val);

    date_time_edit->setMinimumDateTime(QDateTime::fromSecsSinceEpoch(0));

    date_time_edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    checkbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->insertWidget(1, date_time_edit);

    if (Settings::IsConfiguringGlobal()) {
        load_func = [=]() {
            checkbox_load_func();
            if (checkbox->checkState() == Qt::Unchecked) {
                return;
            }

            other_setting->LoadString(
                std::to_string(date_time_edit->dateTime().toSecsSinceEpoch()));
        };
    } else {
        auto get_clear_val = [=]() {
            return QDateTime::fromSecsSinceEpoch([=]() {
                if (checkbox->checkState() == Qt::Checked) {
                    return std::stoll(other_setting->ToStringGlobal());
                }
                return current_time;
            }());
        };

        QObject::connect(restore_button, &QAbstractButton::clicked,
                         [=](bool) { date_time_edit->setDateTime(get_clear_val()); });

        QObject::connect(date_time_edit, &QDateTimeEdit::editingFinished, [=]() {
            if (date_time_edit->dateTime() != get_clear_val()) {
                restore_button->setEnabled(true);
                restore_button->setVisible(true);
            }
        });

        load_func = [=]() {
            checkbox_load_func();
            if (checkbox->checkState() == Qt::Unchecked) {
                return;
            }

            const bool using_global = !restore_button->isEnabled();
            other_setting->SetGlobal(using_global);
            if (!using_global) {
                other_setting->LoadString(
                    std::to_string(date_time_edit->dateTime().toSecsSinceEpoch()));
            }
        };
    }
}

bool Widget::Valid() {
    return created;
}

Widget::~Widget() = default;

Widget::Widget(Settings::BasicSetting* setting_, const TranslationMap& translations_,
               QWidget* parent_, bool runtime_lock,
               std::forward_list<std::function<void(bool)>>& apply_funcs, RequestType request,
               bool managed, float multiplier, Settings::BasicSetting* other_setting,
               const std::string& string)
    : QWidget(parent_), parent{parent_}, translations{translations_}, setting{*setting_} {
    if (!Settings::IsConfiguringGlobal() && !setting.Switchable()) {
        LOG_DEBUG(Frontend, "\"{}\" is not switchable, skipping...", setting.GetLabel());
        return;
    }

    const auto type = setting.TypeId();
    const int id = setting.Id();

    const auto [label, tooltip] = [&]() {
        const auto& setting_label = setting.GetLabel();
        if (translations.contains(id)) {
            return std::pair{translations.at(id).first, translations.at(id).second};
        }
        LOG_WARNING(Frontend, "Translation table lacks entry for \"{}\"", setting_label);
        return std::pair{QString::fromStdString(setting_label), QStringLiteral("")};
    }();

    if (label == QStringLiteral("")) {
        LOG_DEBUG(Frontend, "Translation table has emtpy entry for \"{}\", skipping...",
                  setting.GetLabel());
        return;
    }

    std::function<void()> load_func = []() {};

    if (type == typeid(bool)) {
        switch (request) {
        case RequestType::Default:
            CreateCheckBox(label, load_func);
            break;
        case RequestType::SpinBox:
            CreateCheckBoxWithSpinBox(label, other_setting, load_func, string);
            break;
        case RequestType::HexEdit:
            CreateCheckBoxWithHexEdit(label, other_setting, load_func);
            break;
        case RequestType::LineEdit:
            CreateCheckBoxWithLineEdit(label, other_setting, load_func);
            break;
        case RequestType::DateTimeEdit:
            CreateCheckBoxWithDateTimeEdit(label, other_setting, load_func);
            break;
        case RequestType::ComboBox:
        case RequestType::Slider:
        case RequestType::ReverseSlider:
        case RequestType::MaxEnum:
            LOG_DEBUG(Frontend, "Requested widget is unimplemented.");
            break;
        }
    } else if (setting.IsEnum()) {
        CreateCombobox(label, managed, load_func);
    } else if (type == typeid(u32) || type == typeid(int)) {
        switch (request) {
        case RequestType::Slider:
        case RequestType::ReverseSlider:
            CreateSlider(label, request == RequestType::ReverseSlider, multiplier, load_func);
            break;
        case RequestType::LineEdit:
        case RequestType::Default:
            CreateLineEdit(label, managed, load_func);
            break;
        case RequestType::ComboBox:
            CreateCombobox(label, managed, load_func);
            break;
        case RequestType::DateTimeEdit:
        case RequestType::SpinBox:
        case RequestType::HexEdit:
        case RequestType::MaxEnum:
            LOG_DEBUG(Frontend, "Requested widget is unimplemented.");
            break;
        }
    } else if (type == typeid(std::string)) {
        CreateLineEdit(label, managed, load_func);
    }

    if (!created) {
        LOG_WARNING(Frontend, "No widget was created for \"{}\"", setting.GetLabel());
        return;
    }

    apply_funcs.push_front([load_func, setting_](bool powered_on) {
        if (setting_->RuntimeModfiable() || !powered_on) {
            load_func();
        }
    });

    bool enable = runtime_lock || setting.RuntimeModfiable();
    if (setting.Switchable() && Settings::IsConfiguringGlobal() && !runtime_lock) {
        enable &= setting.UsingGlobal();
    }
    this->setEnabled(enable);

    this->setVisible(Settings::IsConfiguringGlobal() || setting.Switchable());

    this->setToolTip(tooltip);
}

} // namespace ConfigurationShared
