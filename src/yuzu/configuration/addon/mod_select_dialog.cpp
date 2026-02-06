// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFileInfo>
#include <qnamespace.h>
#include "mod_select_dialog.h"
#include "ui_mod_select_dialog.h"

ModSelectDialog::ModSelectDialog(const QStringList& mods, QWidget* parent)
    : QDialog(parent), ui(new Ui::ModSelectDialog) {
    ui->setupUi(this);

    item_model = new QStandardItemModel(ui->treeView);
    ui->treeView->setModel(item_model);

    // We must register all custom types with the Qt Automoc system so that we are able to use it
    // with signals/slots. In this case, QList falls under the umbrella of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    for (const auto& mod : mods) {
        const auto basename = QFileInfo(mod).fileName();

        auto* const first_item = new QStandardItem;
        first_item->setText(basename);
        first_item->setData(mod);

        first_item->setCheckable(true);
        first_item->setCheckState(Qt::Checked);

        item_model->appendRow(first_item);
    }

    ui->treeView->expandAll();
    ui->treeView->resizeColumnToContents(0);

    int rows = item_model->rowCount();
    int height =
        ui->treeView->contentsMargins().top() * 4 + ui->treeView->contentsMargins().bottom() * 4;
    int width = 0;

    for (int i = 0; i < rows; ++i) {
        height += ui->treeView->sizeHintForRow(i);
        width = qMax(width, item_model->item(i)->sizeHint().width());
    }

    width += ui->treeView->contentsMargins().left() * 4 + ui->treeView->contentsMargins().right() * 4;
    ui->treeView->setMinimumHeight(qMin(height, 600));
    ui->treeView->setMinimumWidth(qMin(width, 700));
    adjustSize();

    connect(this, &QDialog::accepted, this, [this]() {
        QStringList selected_mods;

        for (qsizetype i = 0; i < item_model->rowCount(); ++i) {
            auto* const item = item_model->item(i);
            if (item->checkState() == Qt::Checked)
                selected_mods << item->data().toString();
        }

        emit modsSelected(selected_mods);
    });
}

ModSelectDialog::~ModSelectDialog() {
    delete ui;
}
