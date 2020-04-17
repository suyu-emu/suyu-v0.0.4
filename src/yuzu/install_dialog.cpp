// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include "yuzu/install_dialog.h"
#include "yuzu/uisettings.h"

InstallDialog::InstallDialog(QWidget* parent, const QStringList& files) : QDialog(parent) {
    file_list = new QListWidget(this);

    for (const QString& file : files) {
        QListWidgetItem* item = new QListWidgetItem(QFileInfo(file).fileName(), file_list);
        item->setData(Qt::UserRole, file);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }

    file_list->setMinimumWidth((file_list->sizeHintForColumn(0) * 6) / 5);

    vbox_layout = new QVBoxLayout;

    hbox_layout = new QHBoxLayout;

    description = new QLabel(tr("Please confirm these are the files you wish to install."));

    overwrite_files = new QCheckBox(tr("Overwrite Existing Files"));
    overwrite_files->setCheckState(Qt::Unchecked);

    buttons = new QDialogButtonBox;
    buttons->addButton(QDialogButtonBox::Cancel);
    buttons->addButton(tr("Install"), QDialogButtonBox::AcceptRole);

    connect(buttons, &QDialogButtonBox::accepted, this, &InstallDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &InstallDialog::reject);

    hbox_layout->addWidget(overwrite_files);
    hbox_layout->addWidget(buttons);

    vbox_layout->addWidget(description);
    vbox_layout->addWidget(file_list);
    vbox_layout->addLayout(hbox_layout);

    setLayout(vbox_layout);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowTitle(tr("Install Files to NAND"));
}

InstallDialog::~InstallDialog() = default;

QStringList InstallDialog::GetFilenames() const {
    QStringList filenames;

    for (int i = 0; i < file_list->count(); ++i) {
        const QListWidgetItem* item = file_list->item(i);
        if (item->checkState() == Qt::Checked) {
            filenames.append(item->data(Qt::UserRole).toString());
        }
    }

    return filenames;
}

bool InstallDialog::ShouldOverwriteFiles() const {
    return overwrite_files->isChecked();
}