// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "data_dialog.h"
#include "core/hle/service/acc/profile_manager.h"
#include "frontend_common/data_manager.h"
#include "qt_common/qt_common.h"
#include "qt_common/util/content.h"
#include "qt_common/qt_string_lookup.h"
#include "ui_data_dialog.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QProgressDialog>
#include <QtConcurrentRun>

#include <core/frontend/applets/profile_select.h>

#include <applets/qt_profile_select.h>

DataDialog::DataDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::DataDialog>())
{
    ui->setupUi(this);

    // TODO: Should we make this a single widget that pulls data from a model?
#define WIDGET(name) \
    ui->page->addWidget(new DataWidget(FrontendCommon::DataManager::DataDir::name, \
                                       QtCommon::StringLookup::name##Tooltip, \
                                       QStringLiteral(#name), \
                                       this));

    WIDGET(Saves)
    WIDGET(Shaders)
    WIDGET(UserNand)
    WIDGET(SysNand)
    WIDGET(Mods)

#undef WIDGET

    connect(ui->labels, &QListWidget::itemSelectionChanged, this, [this]() {
        const auto items = ui->labels->selectedItems();
        if (items.isEmpty()) {
            return;
        }

        ui->page->setCurrentIndex(ui->labels->row(items[0]));
    });
}

DataDialog::~DataDialog() = default;

DataWidget::DataWidget(FrontendCommon::DataManager::DataDir data_dir,
                       QtCommon::StringLookup::StringKey tooltip,
                       const QString &exportName,
                       QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::DataWidget>())
    , m_dir(data_dir)
    , m_exportName(exportName)
{
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

void DataWidget::clear()
{
    std::string user_id{};
    if (m_dir == FrontendCommon::DataManager::DataDir::Saves) {
        user_id = selectProfile();
    }
    QtCommon::Content::ClearDataDir(m_dir, user_id);
    scan();
}

void DataWidget::open()
{
    std::string user_id{};
    if (m_dir == FrontendCommon::DataManager::DataDir::Saves) {
        user_id = selectProfile();
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(FrontendCommon::DataManager::GetDataDirString(m_dir, user_id))));
}

void DataWidget::upload()
{
    std::string user_id{};
    if (m_dir == FrontendCommon::DataManager::DataDir::Saves) {
        user_id = selectProfile();
    }
    QtCommon::Content::ExportDataDir(m_dir, user_id, m_exportName);
}

void DataWidget::download()
{
    std::string user_id{};
    if (m_dir == FrontendCommon::DataManager::DataDir::Saves) {
        user_id = selectProfile();
    }
    QtCommon::Content::ImportDataDir(m_dir, user_id, std::bind(&DataWidget::scan, this));
}

void DataWidget::scan() {
    ui->size->setText(tr("Calculating..."));

    QFutureWatcher<u64> *watcher = new QFutureWatcher<u64>(this);

    connect(watcher, &QFutureWatcher<u64>::finished, this, [=, this]() {
        u64 size = watcher->result();
        ui->size->setText(
            QString::fromStdString(FrontendCommon::DataManager::ReadableBytesSize(size)));
        watcher->deleteLater();
    });

    watcher->setFuture(
        QtConcurrent::run([this]() { return FrontendCommon::DataManager::DataDirSize(m_dir); }));
}

std::string DataWidget::selectProfile()
{
    const auto select_profile = [this] {
        const Core::Frontend::ProfileSelectParameters parameters{
            .mode = Service::AM::Frontend::UiMode::UserSelector,
            .invalid_uid_list = {},
            .display_options = {},
            .purpose = Service::AM::Frontend::UserSelectionPurpose::General,
        };
        QtProfileSelectionDialog dialog(*QtCommon::system, this, parameters);
        dialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint
                              | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
        dialog.setWindowModality(Qt::WindowModal);

        if (dialog.exec() == QDialog::Rejected) {
            return -1;
        }

        return dialog.GetIndex();
    };

    const auto index = select_profile();
    if (index == -1) {
        return "";
    }

    const auto uuid = QtCommon::system->GetProfileManager().GetUser(static_cast<std::size_t>(index));
    ASSERT(uuid);

    const auto user_id = uuid->AsU128();

    return fmt::format("{:016X}{:016X}", user_id[1], user_id[0]);
}
