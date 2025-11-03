// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DATA_DIALOG_H
#define DATA_DIALOG_H

#include <QDialog>
#include "frontend_common/data_manager.h"
#include "qt_common/qt_string_lookup.h"

#include "ui_data_widget.h"

namespace Ui {
class DataDialog;
}

class DataDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DataDialog(QWidget *parent = nullptr);
    ~DataDialog();

private:
    std::unique_ptr<Ui::DataDialog> ui;
};

class DataWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DataWidget(FrontendCommon::DataManager::DataDir data_dir,
                        QtCommon::StringLookup::StringKey tooltip,
                        const QString &exportName,
                        QWidget *parent = nullptr);

public slots:
    void clear();
    void open();
    void upload();
    void download();

    void scan();

private:
    std::unique_ptr<Ui::DataWidget> ui;
    FrontendCommon::DataManager::DataDir m_dir;
    const QString m_exportName;

    std::string selectProfile();
};

#endif // DATA_DIALOG_H
