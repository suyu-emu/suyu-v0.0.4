// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "data_dialog.h"
#include "frontend_common/data_manager.h"
#include "qt_common/qt_string_lookup.h"
#include "qt_common/util/content.h"
#include "ui_data_dialog.h"
#include "util/util.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QProgressDialog>
#include <QtConcurrentRun>

#include <core/frontend/applets/profile_select.h>

#include <applets/qt_profile_select.h>

DataDialog::DataDialog(QWidget* parent) : QDialog(parent), ui(std::make_unique<Ui::DataDialog>()) {
    ui->setupUi(this);

    // TODO: Should we make this a single widget that pulls data from a model?
#define WIDGET(label, name)                                                                        \
    ui->pages->addTab(new DataWidget(FrontendCommon::DataManager::DataDir::name,                   \
                                     QtCommon::StringLookup::DataManager##name##Tooltip,           \
                                     QStringLiteral(#name), this),                                 \
                      label);

    WIDGET(tr("Shaders"), Shaders)
    WIDGET(tr("User NAND"), UserNand)
    WIDGET(tr("System NAND"), SysNand)
    WIDGET(tr("Mods"), Mods)
    WIDGET(tr("Saves"), Saves)

#undef WIDGET
}

DataDialog::~DataDialog() = default;

DataWidget::DataWidget(FrontendCommon::DataManager::DataDir data_dir,
                       QtCommon::StringLookup::StringKey tooltip, const QString& exportName,
                       QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::DataWidget>()), m_dir(data_dir),
      m_exportName(exportName) {
    ui->setupUi(this);

    ui->tooltip->setText(QtCommon::StringLookup::Lookup(tooltip));

    ui->clear->setIcon(QIcon::fromTheme(QStringLiteral("user-trash")));
    ui->open->setIcon(QIcon::fromTheme(QStringLiteral("folder")));
    ui->upload->setIcon(QIcon::fromTheme(QStringLiteral("upload")));
    ui->download->setIcon(QIcon::fromTheme(QStringLiteral("download")));

    connect(ui->clear, &QPushButton::clicked, this, &DataWidget::clear);
    connect(ui->open, &QPushButton::clicked, this, &DataWidget::open);
    connect(ui->upload, &QPushButton::clicked, this, &DataWidget::upload);
    connect(ui->download, &QPushButton::clicked, this, &DataWidget::download);

    scan();
}

void DataWidget::clear() {
    std::optional<std::string> user_id = selectProfile();
    if (!user_id) return;
    QtCommon::Content::ClearDataDir(m_dir, user_id.value());
    scan();
}

void DataWidget::open() {
    std::optional<std::string> user_id = selectProfile();
    if (!user_id) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(FrontendCommon::DataManager::GetDataDirString(m_dir, user_id.value()))));
}

void DataWidget::upload() {
    std::optional<std::string> user_id = selectProfile();
    if (!user_id) return;
    QtCommon::Content::ExportDataDir(m_dir, user_id.value(), m_exportName);
}

void DataWidget::download() {
    std::optional<std::string> user_id = selectProfile();
    if (!user_id) return;
    QtCommon::Content::ImportDataDir(m_dir, user_id.value(), std::bind(&DataWidget::scan, this));
}

void DataWidget::scan() {
    ui->size->setText(tr("Calculating..."));

    QFutureWatcher<u64>* watcher = new QFutureWatcher<u64>(this);

    connect(watcher, &QFutureWatcher<u64>::finished, this, [=, this]() {
        u64 size = watcher->result();
        ui->size->setText(
            QString::fromStdString(FrontendCommon::DataManager::ReadableBytesSize(size)));
        watcher->deleteLater();
    });

    watcher->setFuture(
        QtConcurrent::run([this]() { return FrontendCommon::DataManager::DataDirSize(m_dir); }));
}

std::optional<std::string> DataWidget::selectProfile() {
    std::string user_id{};
    if (m_dir == FrontendCommon::DataManager::DataDir::Saves) {
        user_id = GetProfileIDString();
        if (user_id.empty()) return std::nullopt;
    }

    return user_id;
}
