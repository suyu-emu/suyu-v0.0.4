// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "ui_configure_input_dialog.h"
#include "yuzu/configuration/configure_input_dialog.h"

ConfigureInputDialog::ConfigureInputDialog(QWidget* parent, std::size_t max_players,
                                           InputCommon::InputSubsystem* input_subsystem)
    : QDialog(parent), ui(std::make_unique<Ui::ConfigureInputDialog>()),
      input_widget(new ConfigureInput(this)) {
    ui->setupUi(this);

    input_widget->Initialize(input_subsystem, max_players);

    ui->inputLayout->addWidget(input_widget);

    RetranslateUI();
}

ConfigureInputDialog::~ConfigureInputDialog() = default;

void ConfigureInputDialog::ApplyConfiguration() {
    input_widget->ApplyConfiguration();
}

void ConfigureInputDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QDialog::changeEvent(event);
}

void ConfigureInputDialog::RetranslateUI() {
    ui->retranslateUi(this);
}
