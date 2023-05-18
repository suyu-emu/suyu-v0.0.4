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
#include <qboxlayout.h>
#include <qnamespace.h>
#include <qpushbutton.h>
#include <qvalidator.h>
#include "common/common_types.h"
#include "common/settings.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/shared_translation.h"
#include "yuzu/configuration/shared_widget.h"

namespace ConfigurationShared {

QPushButton* Widget::CreateRestoreGlobalButton(Settings::BasicSetting& setting, QWidget* parent) {
    QStyle* style = parent->style();
    QIcon* icon = new QIcon(style->standardIcon(QStyle::SP_LineEditClearButton));
    QPushButton* restore_button = new QPushButton(*icon, QStringLiteral(""), parent);
    restore_button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    QSizePolicy sp_retain = restore_button->sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    restore_button->setSizePolicy(sp_retain);

    restore_button->setEnabled(!setting.UsingGlobal());
    restore_button->setVisible(!setting.UsingGlobal());

    return restore_button;
}

QLabel* Widget::CreateLabel(const QString& text) {
    QLabel* qt_label = new QLabel(text, this->parent);
    qt_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    return qt_label;
}

QHBoxLayout* Widget::CreateCheckBox(Settings::BasicSetting* bool_setting, const QString& label,
                                    std::function<void()>& load_func, bool managed) {
    created = true;

    QHBoxLayout* layout = new QHBoxLayout(this);

    checkbox = new QCheckBox(label, this);
    checkbox->setCheckState(bool_setting->ToString() == "true" ? Qt::CheckState::Checked
                                                               : Qt::CheckState::Unchecked);
    checkbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->addWidget(checkbox);

    layout->setContentsMargins(0, 0, 0, 0);

    if (!managed) {
        return layout;
    }

    if (Settings::IsConfiguringGlobal()) {
        load_func = [=]() {
            bool_setting->LoadString(checkbox->checkState() == Qt::Checked ? "true" : "false");
        };
    } else {
        restore_button = CreateRestoreGlobalButton(*bool_setting, this);
        layout->addWidget(restore_button);

        QObject::connect(checkbox, &QCheckBox::stateChanged, [=](int) {
            restore_button->setVisible(true);
            restore_button->setEnabled(true);
        });

        QObject::connect(restore_button, &QAbstractButton::clicked, [=](bool) {
            checkbox->setCheckState(bool_setting->ToStringGlobal() == "true" ? Qt::Checked
                                                                             : Qt::Unchecked);
            restore_button->setEnabled(false);
            restore_button->setVisible(false);
        });

        load_func = [=]() {
            bool using_global = !restore_button->isEnabled();
            bool_setting->SetGlobal(using_global);
            if (!using_global) {
                bool_setting->LoadString(checkbox->checkState() == Qt::Checked ? "true" : "false");
            }
        };
    }

    return layout;
}

void Widget::CreateCombobox(const QString& label, std::function<void()>& load_func, bool managed,
                            Settings::BasicSetting* const other_setting) {
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

    if (!managed) {
        return;
    }

    if (Settings::IsConfiguringGlobal()) {
        load_func = [=]() { setting.LoadString(std::to_string(combobox->currentIndex())); };
    } else {
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

void Widget::CreateLineEdit(const QString& label, std::function<void()>& load_func, bool managed,
                            Settings::BasicSetting* other_setting) {
    const bool has_checkbox = other_setting != nullptr;
    if (has_checkbox && other_setting->TypeId() != typeid(bool)) {
        LOG_WARNING(Frontend, "Extra setting requested but setting is not boolean");
        return;
    }

    created = true;

    QHBoxLayout* layout{nullptr};
    std::function<void()> checkbox_load_func = []() {};

    if (has_checkbox) {
        layout = CreateCheckBox(other_setting, label, checkbox_load_func, managed);
    } else {
        layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        QLabel* q_label = CreateLabel(label);
        layout->addWidget(q_label);
    }

    const QString text = QString::fromStdString(setting.ToString());
    line_edit = new QLineEdit(this);
    line_edit->setText(text);

    layout->addWidget(line_edit);

    if (!managed) {
        return;
    }

    if (Settings::IsConfiguringGlobal()) {
        load_func = [=]() {
            checkbox_load_func();

            std::string load_text = line_edit->text().toStdString();
            setting.LoadString(load_text);
        };
    } else {
        if (!has_checkbox) {
            restore_button = CreateRestoreGlobalButton(setting, this);
            layout->addWidget(restore_button);
        }

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
            checkbox_load_func();

            bool using_global = !restore_button->isEnabled();
            setting.SetGlobal(using_global);
            if (!using_global) {
                setting.LoadString(line_edit->text().toStdString());
            }
        };
    }
}

void Widget::CreateSlider(const QString& label, bool reversed, float multiplier,
                          std::function<void()>& load_func, bool managed,
                          Settings::BasicSetting* const other_setting) {
    created = true;

    QHBoxLayout* layout = new QHBoxLayout(this);
    slider = new QSlider(Qt::Horizontal, this);
    QLabel* qt_label = new QLabel(label, this);
    QLabel* feedback = new QLabel(this);

    layout->addWidget(qt_label);
    layout->addWidget(slider);
    layout->addWidget(feedback);

    qt_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

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

    slider->setInvertedAppearance(reversed);

    if (!managed) {
        return;
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

        QObject::connect(slider, &QAbstractSlider::valueChanged, [=]() {
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

void Widget::CreateSpinBox(const QString& label, std::function<void()>& load_func, bool managed,
                           const std::string& suffix, Settings::BasicSetting* other_setting) {
    const bool has_checkbox = other_setting != nullptr;
    if (has_checkbox && other_setting->TypeId() != typeid(bool)) {
        LOG_WARNING(Frontend, "Extra setting requested but setting is not boolean");
        return;
    }
    created = true;

    QHBoxLayout* layout{nullptr};
    std::function<void()> checkbox_load_func = []() {};
    QLabel* q_label{nullptr};

    if (has_checkbox) {
        layout = CreateCheckBox(other_setting, label, checkbox_load_func, managed);
    } else {
        layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        q_label = CreateLabel(label);
        layout->addWidget(q_label);
    }

    const int min_val = std::stoi(setting.MinVal());
    const int max_val = std::stoi(setting.MaxVal());
    const int default_val = std::stoi(setting.ToString());

    spinbox = new QSpinBox(this);
    spinbox->setRange(min_val, max_val);
    spinbox->setValue(default_val);
    spinbox->setSuffix(QString::fromStdString(suffix));
    spinbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->insertWidget(1, spinbox);

    if (Settings::IsConfiguringGlobal()) {
        load_func = [=]() {
            checkbox_load_func();
            setting.LoadString(std::to_string(spinbox->value()));
        };
    } else {
        if (!has_checkbox) {
            restore_button = CreateRestoreGlobalButton(setting, this);
        }

        QObject::connect(restore_button, &QAbstractButton::clicked,
                         [this](bool) { spinbox->setValue(std::stoi(setting.ToStringGlobal())); });

        QObject::connect(spinbox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int) {
            restore_button->setEnabled(true);
            restore_button->setVisible(true);
        });

        load_func = [=]() {
            checkbox_load_func();

            const bool using_global = !restore_button->isEnabled();
            setting.SetGlobal(using_global);
            if (!using_global) {
                setting.LoadString(std::to_string(spinbox->value()));
            }
        };
    }
}

void Widget::CreateHexEdit(const QString& label, std::function<void()>& load_func, bool managed,
                           Settings::BasicSetting* const other_setting) {
    CreateLineEdit(label, load_func, false, other_setting);
    if (!created || !managed) {
        return;
    }

    QLayout* layout = this->layout();

    auto to_hex = [=](const std::string& input) {
        return QString::fromStdString(fmt::format("{:08x}", std::stoi(input)));
    };

    QRegExpValidator* regex =
        new QRegExpValidator{QRegExp{QStringLiteral("^[0-9a-fA-F]{0,8}$")}, line_edit};

    const QString default_val = to_hex(setting.ToString());

    line_edit->setText(default_val);
    line_edit->setMaxLength(8);
    line_edit->setValidator(regex);

    auto hex_to_dec = [=]() -> std::string {
        return std::to_string(std::stoul(line_edit->text().toStdString(), nullptr, 16));
    };

    if (Settings::IsConfiguringGlobal()) {
        load_func = [=]() {
            other_setting->LoadString(checkbox->checkState() == Qt::Checked ? "true" : "false");
            setting.LoadString(hex_to_dec());
        };
    } else {
        restore_button = CreateRestoreGlobalButton(setting, this);
        layout->addWidget(restore_button);

        QObject::connect(restore_button, &QAbstractButton::clicked, [=](bool) {
            line_edit->setText(to_hex(setting.ToStringGlobal()));
            checkbox->setCheckState(other_setting->ToStringGlobal() == "true" ? Qt::Checked
                                                                              : Qt::Unchecked);

            restore_button->setEnabled(false);
            restore_button->setVisible(false);
        });

        QObject::connect(line_edit, &QLineEdit::textEdited, [&]() {
            restore_button->setEnabled(true);
            restore_button->setVisible(true);
        });

        QObject::connect(checkbox, &QAbstractButton::clicked, [&]() {
            restore_button->setEnabled(true);
            restore_button->setVisible(true);
        });

        load_func = [=]() {
            const bool using_global = !restore_button->isEnabled();
            other_setting->SetGlobal(using_global);
            setting.SetGlobal(using_global);

            if (!using_global) {
                other_setting->LoadString(checkbox->checkState() == Qt::Checked ? "true" : "false");
                setting.LoadString(hex_to_dec());
            }
        };
    }
}

void Widget::CreateDateTimeEdit(const QString& label, std::function<void()>& load_func,
                                bool managed, bool restrict,
                                Settings::BasicSetting* const other_setting) {
    const bool has_checkbox = other_setting != nullptr;
    if ((restrict && !has_checkbox) || (has_checkbox && other_setting->TypeId() != typeid(bool))) {
        LOG_WARNING(Frontend, "Extra setting or restrict requested but is not boolean");
        return;
    }
    created = true;

    QHBoxLayout* layout{nullptr};
    std::function<void()> checkbox_load_func = []() {};

    if (has_checkbox) {
        layout = CreateCheckBox(other_setting, label, checkbox_load_func, managed);
    } else {
        layout = new QHBoxLayout(this);
        QLabel* q_label = CreateLabel(label);
        layout->addWidget(q_label);
    }

    const bool disabled = other_setting->ToString() != "true";
    const long long current_time = QDateTime::currentSecsSinceEpoch();
    const s64 the_time = disabled ? current_time : std::stoll(setting.ToString());
    const auto default_val = QDateTime::fromSecsSinceEpoch(the_time);

    date_time_edit = new QDateTimeEdit(this);
    date_time_edit->setDateTime(default_val);
    date_time_edit->setMinimumDateTime(QDateTime::fromSecsSinceEpoch(0));
    date_time_edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->insertWidget(1, date_time_edit);

    if (!managed) {
        return;
    }

    if (Settings::IsConfiguringGlobal()) {
        load_func = [=]() {
            checkbox_load_func();
            if (restrict && checkbox->checkState() == Qt::Unchecked) {
                return;
            }

            setting.LoadString(std::to_string(date_time_edit->dateTime().toSecsSinceEpoch()));
        };
    } else {
        if (!has_checkbox) {
            restore_button = CreateRestoreGlobalButton(setting, this);
            layout->addWidget(restore_button);
        }

        auto get_clear_val = [=]() {
            return QDateTime::fromSecsSinceEpoch([=]() {
                if (restrict && checkbox->checkState() == Qt::Checked) {
                    return std::stoll(setting.ToStringGlobal());
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
            if (restrict && checkbox->checkState() == Qt::Unchecked) {
                return;
            }

            const bool using_global = !restore_button->isEnabled();
            other_setting->SetGlobal(using_global);
            if (!using_global) {
                setting.LoadString(std::to_string(date_time_edit->dateTime().toSecsSinceEpoch()));
            }
        };
    }
}

bool Widget::Valid() {
    return created;
}

Widget::~Widget() = default;

Widget::Widget(Settings::BasicSetting* setting_, const TranslationMap& translations_,
               QWidget* parent_, std::forward_list<std::function<void(bool)>>& apply_funcs_)
    : QWidget(parent_), parent{parent_}, translations{translations_}, setting{*setting_},
      apply_funcs{apply_funcs_} {}

Widget::Widget(Settings::BasicSetting* setting_, const TranslationMap& translations_,
               QWidget* parent_, bool runtime_lock,
               std::forward_list<std::function<void(bool)>>& apply_funcs_, RequestType request,
               bool managed, float multiplier, Settings::BasicSetting* other_setting,
               const std::string& string)
    : QWidget(parent_), parent{parent_}, translations{translations_}, setting{*setting_},
      apply_funcs{apply_funcs_} {
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
            CreateCheckBox(&setting, label, load_func, managed);
            break;
        default:
            LOG_WARNING(Frontend, "Requested widget is unimplemented.");
            break;
        }
    } else if (setting.IsEnum()) {
        CreateCombobox(label, load_func, managed);
    } else if (type == typeid(u32) || type == typeid(int) || type == typeid(u16) ||
               type == typeid(s64)) {
        switch (request) {
        case RequestType::Slider:
        case RequestType::ReverseSlider:
            CreateSlider(label, request == RequestType::ReverseSlider, multiplier, load_func,
                         managed);
            break;
        case RequestType::LineEdit:
        case RequestType::Default:
            CreateLineEdit(label, load_func, managed);
            break;
        case RequestType::ComboBox:
            CreateCombobox(label, load_func, managed);
            break;
        case RequestType::DateTimeEdit:
            CreateDateTimeEdit(label, load_func, managed, true, other_setting);
            break;
        case RequestType::SpinBox:
            CreateSpinBox(label, load_func, managed, string, other_setting);
            break;
        case RequestType::HexEdit:
            CreateHexEdit(label, load_func, managed, other_setting);
            break;
        default:
            LOG_WARNING(Frontend, "Requested widget is unimplemented.");
            break;
        }
    } else if (type == typeid(std::string)) {
        CreateLineEdit(label, load_func, managed);
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
