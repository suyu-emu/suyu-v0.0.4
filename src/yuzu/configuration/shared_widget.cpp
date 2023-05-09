#include <functional>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QWidget>
#include <qabstractbutton.h>
#include "common/settings.h"
#include "yuzu/configuration/shared_translation.h"
#include "yuzu/configuration/shared_widget.h"

namespace ConfigurationShared {

QPushButton* Widget::CreateRestoreGlobalButton(Settings::BasicSetting& setting, QWidget* parent) {
    QStyle* style = parent->style();
    QIcon* icon = new QIcon(style->standardIcon(QStyle::SP_DialogResetButton));
    QPushButton* restore_button = new QPushButton(*icon, QStringLiteral(""), parent);
    restore_button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);

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

void Widget::CreateCheckBoxWithLineEdit(const QString& label,
                                        const Settings::BasicSetting* other_setting,
                                        std::function<void()>& load_func) {
    created = true;

    CreateCheckBox(label, load_func);

    QHBoxLayout* layout = reinterpret_cast<QHBoxLayout*>(this->layout());

    line_edit = new QLineEdit(this);
    line_edit->setText(QString::fromStdString(other_setting->ToString()));

    checkbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->insertWidget(1, line_edit);

    QObject::connect(line_edit, &QLineEdit::textEdited,
                     [=](const QString&) { checkbox->setCheckState(Qt::Checked); });

    if (!Settings::IsConfiguringGlobal()) {
        QObject::connect(restore_button, &QAbstractButton::clicked, [=](bool) {
            line_edit->setText(QString::fromStdString(other_setting->ToString()));
        });

        QObject::connect(line_edit, &QLineEdit::textEdited, [=](const QString&) {
            restore_button->setEnabled(true);
            restore_button->setVisible(true);
        });
    }
}

void Widget::CreateCheckBoxWithSpinBox(const QString& label,
                                       const Settings::BasicSetting* other_setting,
                                       std::function<void()>& load_func, const QString& suffix) {
    created = true;

    CreateCheckBox(label, load_func);
    checkbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QHBoxLayout* layout = reinterpret_cast<QHBoxLayout*>(this->layout());

    spinbox = new QSpinBox(this);
    const int min_val = std::stoi(other_setting->MinVal());
    const int max_val = std::stoi(other_setting->MaxVal());
    spinbox->setRange(min_val, max_val);
    spinbox->setValue(std::stoi(other_setting->ToString()));
    spinbox->setSuffix(suffix);
    spinbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->insertWidget(1, spinbox);

    QObject::connect(spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
                     [this](int) { checkbox->setCheckState(Qt::Checked); });

    if (!Settings::IsConfiguringGlobal()) {
        QObject::connect(restore_button, &QAbstractButton::clicked, [this, other_setting](bool) {
            spinbox->setValue(std::stoi(other_setting->ToString()));
        });

        QObject::connect(spinbox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int) {
            restore_button->setEnabled(true);
            restore_button->setVisible(true);
        });
    }
}

bool Widget::Valid() {
    return created;
}

Widget::~Widget() = default;

Widget::Widget(Settings::BasicSetting* setting_, const TranslationMap& translations_,
               QWidget* parent_, bool runtime_lock,
               std::forward_list<std::function<void(bool)>>& apply_funcs, RequestType request,
               bool managed, float multiplier, const Settings::BasicSetting* other_setting,
               const QString& format)
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
        case RequestType::LineEdit:
            CreateCheckBoxWithLineEdit(label, other_setting, load_func);
            break;
        case RequestType::SpinBox:
            CreateCheckBoxWithSpinBox(label, other_setting, load_func, format);
            break;
        case RequestType::ComboBox:
        case RequestType::Slider:
        case RequestType::ReverseSlider:
        case RequestType::MaxEnum:
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
        case RequestType::SpinBox:
        case RequestType::MaxEnum:
            break;
        }
    }

    if (!created) {
        LOG_ERROR(Frontend, "No widget was created for \"{}\"", setting.GetLabel());
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
