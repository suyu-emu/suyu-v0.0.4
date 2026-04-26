// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <typeinfo>
#include <vector>
#include <QComboBox>
#include <QStandardItemModel>
#include "common/common_types.h"
#include "common/settings.h"
#include "common/settings_enums.h"
#include "configuration/shared_widget.h"
#include "core/core.h"
#include "suyu/configuration/configuration_shared.h"
#include "suyu/configuration/configure_cpu.h"
#include "ui_configure_cpu.h"

ConfigureCpu::ConfigureCpu(const Core::System& system_,
                           std::shared_ptr<std::vector<ConfigurationShared::Tab*>> group_,
                           const ConfigurationShared::Builder& builder, QWidget* parent)
    : Tab(group_, parent), ui{std::make_unique<Ui::ConfigureCpu>()}, system{system_},
      combobox_translations(builder.ComboboxTranslations()) {
    ui->setupUi(this);

    Setup(builder);

    SetConfiguration();

    connect(accuracy_combobox, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ConfigureCpu::UpdateGroup);

    const auto connect_backend = [this](QComboBox* combobox) {
        if (combobox == nullptr) {
            return;
        }
        connect(combobox, qOverload<int>(&QComboBox::currentIndexChanged), this,
                &ConfigureCpu::UpdateAvailabilityUi);
    };
    connect_backend(execution_path_combobox);
    connect_backend(recompiler_combobox);
    connect_backend(core_provider_combobox);

    ui->backend_group->setVisible(true);
    UpdateAvailabilityUi();
}

ConfigureCpu::~ConfigureCpu() = default;

void ConfigureCpu::SetConfiguration() {}
void ConfigureCpu::Setup(const ConfigurationShared::Builder& builder) {
    auto* accuracy_layout = ui->widget_accuracy->layout();
    auto* backend_layout = ui->widget_backend->layout();
    auto* unsafe_layout = ui->unsafe_widget->layout();
    std::map<u32, QWidget*> unsafe_hold{};

    std::vector<Settings::BasicSetting*> settings;
    const auto push = [&](Settings::Category category) {
        for (const auto setting : Settings::values.linkage.by_category[category]) {
            settings.push_back(setting);
        }
    };

    push(Settings::Category::Cpu);
    push(Settings::Category::CpuUnsafe);

    for (const auto setting : settings) {
        auto* widget = builder.BuildWidget(setting, apply_funcs);

        if (widget == nullptr) {
            continue;
        }
        if (!widget->Valid()) {
            widget->deleteLater();
            continue;
        }

        if (setting->Id() == Settings::values.cpu_accuracy.Id()) {
            // Keep track of cpu_accuracy combobox to display/hide the unsafe settings
            accuracy_layout->addWidget(widget);
            accuracy_combobox = widget->combobox;
        } else if (setting->Id() == Settings::values.cpu_execution_path.Id()) {
            backend_layout->addWidget(widget);
            execution_path_combobox = widget->combobox;
        } else if (setting->Id() == Settings::values.cpu_recompiler_engine.Id()) {
            backend_layout->addWidget(widget);
            recompiler_combobox = widget->combobox;
        } else if (setting->Id() == Settings::values.cpu_core_provider.Id()) {
            backend_layout->addWidget(widget);
            core_provider_combobox = widget->combobox;
        } else {
            // Presently, all other settings here are unsafe checkboxes
            unsafe_hold.insert({setting->Id(), widget});
        }
    }

    for (const auto& [_, widget] : unsafe_hold) {
        unsafe_layout->addWidget(widget);
    }

    backend_status_label = new QLabel(this);
    backend_status_label->setWordWrap(true);
    backend_layout->addWidget(backend_status_label);

    UpdateGroup(accuracy_combobox->currentIndex());
}

void ConfigureCpu::UpdateGroup(int index) {
    const auto accuracy = static_cast<Settings::CpuAccuracy>(
        combobox_translations.at(Settings::EnumMetadata<Settings::CpuAccuracy>::Index())[index]
            .first);
    ui->unsafe_group->setVisible(accuracy == Settings::CpuAccuracy::Unsafe);
}

int ConfigureCpu::FindComboboxIndex(u32 enum_index, u32 value) const {
    if (!combobox_translations.contains(enum_index)) {
        return -1;
    }

    const auto& entries = combobox_translations.at(enum_index);
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        if (entries[i].first == value) {
            return i;
        }
    }

    return -1;
}

void ConfigureCpu::SetOptionEnabled(QComboBox* combobox, int index, bool enabled,
                                    const QString& disabled_reason) {
    if (combobox == nullptr || index < 0) {
        return;
    }

    auto* model = qobject_cast<QStandardItemModel*>(combobox->model());
    if (model == nullptr) {
        return;
    }

    auto* item = model->item(index);
    if (item == nullptr) {
        return;
    }

    item->setEnabled(enabled);
    item->setToolTip(enabled ? QStringLiteral() : disabled_reason);
}

void ConfigureCpu::EnsureCurrentSelectionEnabled(QComboBox* combobox) const {
    if (combobox == nullptr) {
        return;
    }

    auto* model = qobject_cast<QStandardItemModel*>(combobox->model());
    if (model == nullptr) {
        return;
    }

    const int current_index = combobox->currentIndex();
    if (current_index >= 0) {
        auto* current_item = model->item(current_index);
        if (current_item != nullptr && current_item->isEnabled()) {
            return;
        }
    }

    for (int i = 0; i < combobox->count(); ++i) {
        auto* item = model->item(i);
        if (item != nullptr && item->isEnabled()) {
            combobox->setCurrentIndex(i);
            return;
        }
    }
}

void ConfigureCpu::UpdateAvailabilityUi() {
    const QString nce_unavailable =
        tr("NCE requires a compatible ARM64 host build and is unavailable here.");
    const QString ballistic_unavailable =
        tr("Ballistic is not linked into this build yet.");
    const QString rem_unavailable = tr("REM is not linked into this build yet.");

    SetOptionEnabled(
        execution_path_combobox,
        FindComboboxIndex(Settings::EnumMetadata<Settings::CpuExecutionPath>::Index(),
                          static_cast<u32>(Settings::CpuExecutionPath::Nce)),
#ifdef HAS_NCE
        true,
#else
        false,
#endif
        nce_unavailable);

    SetOptionEnabled(
        recompiler_combobox,
        FindComboboxIndex(Settings::EnumMetadata<Settings::CpuRecompilerEngine>::Index(),
                          static_cast<u32>(Settings::CpuRecompilerEngine::BallisticExperimental)),
        Settings::IsBallisticAvailable(), ballistic_unavailable);
    SetOptionEnabled(
        core_provider_combobox,
        FindComboboxIndex(Settings::EnumMetadata<Settings::CpuCoreProvider>::Index(),
                          static_cast<u32>(Settings::CpuCoreProvider::RemExperimental)),
        Settings::IsRemAvailable(), rem_unavailable);

    EnsureCurrentSelectionEnabled(execution_path_combobox);
    EnsureCurrentSelectionEnabled(recompiler_combobox);
    EnsureCurrentSelectionEnabled(core_provider_combobox);

    QStringList notes;
#ifndef HAS_NCE
    notes.append(nce_unavailable);
#endif
    if (!Settings::IsBallisticAvailable()) {
        notes.append(ballistic_unavailable);
    }
    if (!Settings::IsRemAvailable()) {
        notes.append(rem_unavailable);
    }

    if (execution_path_combobox != nullptr) {
        const auto execution_path = static_cast<Settings::CpuExecutionPath>(
            combobox_translations
                .at(Settings::EnumMetadata<Settings::CpuExecutionPath>::Index())
                    [execution_path_combobox->currentIndex()]
                .first);
        if (execution_path == Settings::CpuExecutionPath::Nce) {
            notes.append(tr("Recompiler Engine applies to the JIT path only."));
        }
    }

    backend_status_label->setVisible(!notes.isEmpty());
    backend_status_label->setText(notes.join(QLatin1Char('\n')));
}

void ConfigureCpu::ApplyConfiguration() {
    const bool is_powered_on = system.IsPoweredOn();
    for (const auto& apply_func : apply_funcs) {
        apply_func(is_powered_on);
    }
}

void ConfigureCpu::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureCpu::RetranslateUI() {
    ui->retranslateUi(this);
}
