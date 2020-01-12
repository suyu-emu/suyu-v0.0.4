// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QMessageBox>
#include <QStandardItemModel>
#include "core/settings.h"
#include "ui_configure_hotkeys.h"
#include "yuzu/configuration/configure_hotkeys.h"
#include "yuzu/hotkeys.h"
#include "yuzu/util/sequence_dialog/sequence_dialog.h"

ConfigureHotkeys::ConfigureHotkeys(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureHotkeys>()) {
    ui->setupUi(this);
    setFocusPolicy(Qt::ClickFocus);

    model = new QStandardItemModel(this);
    model->setColumnCount(3);

    connect(ui->hotkey_list, &QTreeView::doubleClicked, this, &ConfigureHotkeys::Configure);
    ui->hotkey_list->setModel(model);

    // TODO(Kloen): Make context configurable as well (hiding the column for now)
    ui->hotkey_list->hideColumn(2);

    ui->hotkey_list->setColumnWidth(0, 200);
    ui->hotkey_list->resizeColumnToContents(1);

    RetranslateUI();
}

ConfigureHotkeys::~ConfigureHotkeys() = default;

void ConfigureHotkeys::Populate(const HotkeyRegistry& registry) {
    for (const auto& group : registry.hotkey_groups) {
        auto* parent_item = new QStandardItem(group.first);
        parent_item->setEditable(false);
        for (const auto& hotkey : group.second) {
            auto* action = new QStandardItem(hotkey.first);
            auto* keyseq =
                new QStandardItem(hotkey.second.keyseq.toString(QKeySequence::NativeText));
            action->setEditable(false);
            keyseq->setEditable(false);
            parent_item->appendRow({action, keyseq});
        }
        model->appendRow(parent_item);
    }

    ui->hotkey_list->expandAll();
    ui->hotkey_list->resizeColumnToContents(0);
}

void ConfigureHotkeys::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureHotkeys::RetranslateUI() {
    ui->retranslateUi(this);

    model->setHorizontalHeaderLabels({tr("Action"), tr("Hotkey"), tr("Context")});
}

void ConfigureHotkeys::Configure(QModelIndex index) {
    if (!index.parent().isValid()) {
        return;
    }

    index = index.sibling(index.row(), 1);
    auto* const model = ui->hotkey_list->model();
    const auto previous_key = model->data(index);

    SequenceDialog hotkey_dialog{this};

    const int return_code = hotkey_dialog.exec();
    const auto key_sequence = hotkey_dialog.GetSequence();
    if (return_code == QDialog::Rejected || key_sequence.isEmpty()) {
        return;
    }

    if (IsUsedKey(key_sequence) && key_sequence != QKeySequence(previous_key.toString())) {
        QMessageBox::warning(this, tr("Conflicting Key Sequence"),
                             tr("The entered key sequence is already assigned to another hotkey."));
    } else {
        model->setData(index, key_sequence.toString(QKeySequence::NativeText));
    }
}

bool ConfigureHotkeys::IsUsedKey(QKeySequence key_sequence) const {
    for (int r = 0; r < model->rowCount(); r++) {
        const QStandardItem* const parent = model->item(r, 0);

        for (int r2 = 0; r2 < parent->rowCount(); r2++) {
            const QStandardItem* const key_seq_item = parent->child(r2, 1);
            const auto key_seq_str = key_seq_item->text();
            const auto key_seq = QKeySequence::fromString(key_seq_str, QKeySequence::NativeText);

            if (key_sequence == key_seq) {
                return true;
            }
        }
    }

    return false;
}

void ConfigureHotkeys::ApplyConfiguration(HotkeyRegistry& registry) {
    for (int key_id = 0; key_id < model->rowCount(); key_id++) {
        const QStandardItem* parent = model->item(key_id, 0);
        for (int key_column_id = 0; key_column_id < parent->rowCount(); key_column_id++) {
            const QStandardItem* action = parent->child(key_column_id, 0);
            const QStandardItem* keyseq = parent->child(key_column_id, 1);
            for (auto& [group, sub_actions] : registry.hotkey_groups) {
                if (group != parent->text())
                    continue;
                for (auto& [action_name, hotkey] : sub_actions) {
                    if (action_name != action->text())
                        continue;
                    hotkey.keyseq = QKeySequence(keyseq->text());
                }
            }
        }
    }

    registry.SaveHotkeys();
}
