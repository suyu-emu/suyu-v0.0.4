// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <forward_list>
#include <memory>
#include <QComboBox>

#include "audio_core/sink/sink.h"
#include "audio_core/sink/sink_details.h"
#include "common/settings.h"
#include "core/core.h"
#include "ui_configure_audio.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_audio.h"
#include "yuzu/configuration/shared_translation.h"
#include "yuzu/configuration/shared_widget.h"
#include "yuzu/uisettings.h"

ConfigureAudio::ConfigureAudio(
    const Core::System& system_,
    std::shared_ptr<std::forward_list<ConfigurationShared::Tab*>> group,
    const ConfigurationShared::TranslationMap& translations_,
    const ConfigurationShared::ComboboxTranslationMap& combobox_translations_, QWidget* parent)
    : Tab(group, parent), ui(std::make_unique<Ui::ConfigureAudio>()), system{system_},
      translations{translations_}, combobox_translations{combobox_translations_} {
    ui->setupUi(this);
    Setup();

    SetConfiguration();
}

ConfigureAudio::~ConfigureAudio() = default;

void ConfigureAudio::Setup() {
    const bool runtime_lock = !system.IsPoweredOn();
    auto& layout = *ui->audio_widget->layout();

    std::forward_list<Settings::BasicSetting*> settings;

    auto push = [&](Settings::Category category) {
        for (auto* setting : Settings::values.linkage.by_category[category]) {
            settings.push_front(setting);
        }
    };

    push(Settings::Category::Audio);
    push(Settings::Category::SystemAudio);

    for (auto* setting : settings) {
        auto* widget = [&]() {
            if (setting->Id() == Settings::values.volume.Id()) {
                // volume needs to be a slider (default is line edit)
                return new ConfigurationShared::Widget(
                    setting, translations, combobox_translations, this, runtime_lock, apply_funcs,
                    ConfigurationShared::RequestType::Slider, true, 1.0f, nullptr,
                    tr("%1%", "Volume percentage (e.g. 50%)"));
            } else if (setting->Id() == Settings::values.audio_output_device_id.Id() ||
                       setting->Id() == Settings::values.audio_input_device_id.Id() ||
                       setting->Id() == Settings::values.sink_id.Id()) {
                // These need to be unmanaged comboboxes, so we can populate them ourselves
                return new ConfigurationShared::Widget(
                    setting, translations, combobox_translations, this, runtime_lock, apply_funcs,
                    ConfigurationShared::RequestType::ComboBox, false);
            } else {
                return new ConfigurationShared::Widget(setting, translations, combobox_translations,
                                                       this, runtime_lock, apply_funcs);
            }
        }();

        if (!widget->Valid()) {
            delete widget;
            continue;
        }

        layout.addWidget(widget);

        if (setting->Id() == Settings::values.sink_id.Id()) {
            sink_combo_box = widget->combobox;
            InitializeAudioSinkComboBox();

            connect(sink_combo_box, qOverload<int>(&QComboBox::currentIndexChanged), this,
                    &ConfigureAudio::UpdateAudioDevices);
        } else if (setting->Id() == Settings::values.audio_output_device_id.Id()) {
            // Keep track of output (and input) device comboboxes to populate them with system
            // devices, which are determined at run time
            output_device_combo_box = widget->combobox;
        } else if (setting->Id() == Settings::values.audio_input_device_id.Id()) {
            input_device_combo_box = widget->combobox;
        }
    }
}

void ConfigureAudio::SetConfiguration() {
    if (!Settings::IsConfiguringGlobal()) {
        return;
    }

    SetOutputSinkFromSinkID();

    // The device list cannot be pre-populated (nor listed) until the output sink is known.
    UpdateAudioDevices(sink_combo_box->currentIndex());

    SetAudioDevicesFromDeviceID();
}

void ConfigureAudio::SetOutputSinkFromSinkID() {
    [[maybe_unused]] const QSignalBlocker blocker(sink_combo_box);

    int new_sink_index = 0;
    const QString sink_id = QString::fromStdString(Settings::values.sink_id.ToString());
    for (int index = 0; index < sink_combo_box->count(); index++) {
        if (sink_combo_box->itemText(index) == sink_id) {
            new_sink_index = index;
            break;
        }
    }

    sink_combo_box->setCurrentIndex(new_sink_index);
}

void ConfigureAudio::SetAudioDevicesFromDeviceID() {
    int new_device_index = -1;

    const QString output_device_id =
        QString::fromStdString(Settings::values.audio_output_device_id.GetValue());
    for (int index = 0; index < output_device_combo_box->count(); index++) {
        if (output_device_combo_box->itemText(index) == output_device_id) {
            new_device_index = index;
            break;
        }
    }

    output_device_combo_box->setCurrentIndex(new_device_index);

    new_device_index = -1;
    const QString input_device_id =
        QString::fromStdString(Settings::values.audio_input_device_id.GetValue());
    for (int index = 0; index < input_device_combo_box->count(); index++) {
        if (input_device_combo_box->itemText(index) == input_device_id) {
            new_device_index = index;
            break;
        }
    }

    input_device_combo_box->setCurrentIndex(new_device_index);
}

void ConfigureAudio::ApplyConfiguration() {
    const bool is_powered_on = system.IsPoweredOn();
    for (const auto& apply_func : apply_funcs) {
        apply_func(is_powered_on);
    }

    if (Settings::IsConfiguringGlobal()) {
        Settings::values.sink_id.LoadString(
            sink_combo_box->itemText(sink_combo_box->currentIndex()).toStdString());
        Settings::values.audio_output_device_id.SetValue(
            output_device_combo_box->itemText(output_device_combo_box->currentIndex())
                .toStdString());
        Settings::values.audio_input_device_id.SetValue(
            input_device_combo_box->itemText(input_device_combo_box->currentIndex()).toStdString());
    }
}

void ConfigureAudio::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureAudio::UpdateAudioDevices(int sink_index) {
    output_device_combo_box->clear();
    output_device_combo_box->addItem(QString::fromUtf8(AudioCore::Sink::auto_device_name));

    const auto sink_id =
        Settings::ToEnum<Settings::AudioEngine>(sink_combo_box->itemText(sink_index).toStdString());
    for (const auto& device : AudioCore::Sink::GetDeviceListForSink(sink_id, false)) {
        output_device_combo_box->addItem(QString::fromStdString(device));
    }

    input_device_combo_box->clear();
    input_device_combo_box->addItem(QString::fromUtf8(AudioCore::Sink::auto_device_name));
    for (const auto& device : AudioCore::Sink::GetDeviceListForSink(sink_id, true)) {
        input_device_combo_box->addItem(QString::fromStdString(device));
    }
}

void ConfigureAudio::InitializeAudioSinkComboBox() {
    sink_combo_box->clear();
    sink_combo_box->addItem(QString::fromUtf8(AudioCore::Sink::auto_device_name));

    for (const auto& id : AudioCore::Sink::GetSinkIDs()) {
        sink_combo_box->addItem(QString::fromStdString(Settings::CanonicalizeEnum(id)));
    }
}

void ConfigureAudio::RetranslateUI() {
    ui->retranslateUi(this);
}
