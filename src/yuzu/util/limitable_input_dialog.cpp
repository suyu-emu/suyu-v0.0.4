// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include "yuzu/util/limitable_input_dialog.h"

LimitableInputDialog::LimitableInputDialog(QWidget* parent) : QDialog{parent} {
    CreateUI();
    ConnectEvents();
}

LimitableInputDialog::~LimitableInputDialog() = default;

void LimitableInputDialog::CreateUI() {
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    text_label = new QLabel(this);
    text_entry = new QLineEdit(this);
    buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* const layout = new QVBoxLayout;
    layout->addWidget(text_label);
    layout->addWidget(text_entry);
    layout->addWidget(buttons);

    setLayout(layout);
}

void LimitableInputDialog::ConnectEvents() {
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString LimitableInputDialog::GetText(QWidget* parent, const QString& title, const QString& text,
                                      int min_character_limit, int max_character_limit) {
    Q_ASSERT(min_character_limit <= max_character_limit);

    LimitableInputDialog dialog{parent};
    dialog.setWindowTitle(title);
    dialog.text_label->setText(text);
    dialog.text_entry->setMaxLength(max_character_limit);

    auto* const ok_button = dialog.buttons->button(QDialogButtonBox::Ok);
    ok_button->setEnabled(false);
    connect(dialog.text_entry, &QLineEdit::textEdited, [&](const QString& new_text) {
        ok_button->setEnabled(new_text.length() >= min_character_limit);
    });

    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }

    return dialog.text_entry->text();
}
