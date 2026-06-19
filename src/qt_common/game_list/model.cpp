// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThreadPool>

#include "common/logging.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/qt_common.h"
#include "qt_common/util/game.h"

#include "qt_common/game_list/game_list_p.h"
#include "qt_common/game_list/model.h"
#include "qt_common/game_list/worker.h"

GameListModel::GameListModel(std::shared_ptr<FileSys::VfsFilesystem> vfs_,
                             FileSys::ManualContentProvider* provider_,
                             const PlayTime::PlayTimeManager& play_time_manager_,
                             Core::System& system_, QObject* parent)
    : QStandardItemModel{parent}, vfs{std::move(vfs_)}, provider{provider_},
      play_time_manager{play_time_manager_}, system{system_} {
    watcher = new QFileSystemWatcher(this);
    external_watcher = new QFileSystemWatcher(this);

    connect(watcher, &QFileSystemWatcher::directoryChanged, this,
            &GameListModel::RefreshGameDirectory);
    connect(external_watcher, &QFileSystemWatcher::directoryChanged, this,
            &GameListModel::RefreshExternalContent);

    ResetExternalWatcher();

    insertColumns(0, COLUMN_COUNT);
    RetranslateUI();

    setSortRole(GameListItemPath::SortRole);
}

GameListModel::~GameListModel() = default;

void GameListModel::PopulateAsync(QVector<UISettings::GameDir>& game_dirs) {
    emit PopulatingStarted();

    current_worker.reset();
    removeRows(0, rowCount());

    current_worker = std::make_unique<GameListWorker>(vfs, provider, game_dirs, compatibility_list,
                                                      play_time_manager, system);

    connect(current_worker.get(), &GameListWorker::DataAvailable, this, &GameListModel::WorkerEvent,
            Qt::QueuedConnection);

    QThreadPool::globalInstance()->start(current_worker.get());
}

void GameListModel::WorkerEvent() {
    current_worker->ProcessEvents(this);
}

void GameListModel::AddDirEntry(GameListDir* entry_items) {
    if (m_flat) {
        return;
    }
    invisibleRootItem()->appendRow(entry_items);
}

void GameListModel::AddEntry(const QList<QStandardItem*>& entry_items, GameListDir* parent) {
    if (m_flat) {
        invisibleRootItem()->appendRow(entry_items);
    } else {
        parent->appendRow(entry_items);
    }
}

void GameListModel::DonePopulating(const QStringList& watch_list) {
    emit ShowList(!IsEmpty());

    if (!m_flat) {
        invisibleRootItem()->appendRow(new GameListAddDir());
        invisibleRootItem()->insertRow(0, new GameListFavorites());

        for (const auto id : std::as_const(UISettings::values.favorited_ids)) {
            AddFavorite(id);
        }
    }

    emit PopulatingCompleted(watch_list);
}

bool GameListModel::IsEmpty() const {
    for (int i = 0; i < rowCount(); i++) {
        const QStandardItem* child = invisibleRootItem()->child(i);
        const auto type = static_cast<GameListItemType>(child->type());

        if (!child->hasChildren() &&
            (type == GameListItemType::SdmcDir || type == GameListItemType::UserNandDir ||
             type == GameListItemType::SysNandDir)) {
            invisibleRootItem()->removeRow(child->row());
            i--;
        }
    }

    return !invisibleRootItem()->hasChildren();
}

void GameListModel::ToggleFavorite(u64 program_id) {
    if (!UISettings::values.favorited_ids.contains(program_id)) {
        UISettings::values.favorited_ids.append(program_id);
        AddFavorite(program_id);
    } else {
        UISettings::values.favorited_ids.removeOne(program_id);
        RemoveFavorite(program_id);
    }
    emit SaveConfig();
}

void GameListModel::AddFavorite(u64 program_id) {
    auto* favorites_row = item(0);

    for (int i = 1; i < rowCount() - 1; i++) {
        const auto* folder = item(i);
        for (int j = 0; j < folder->rowCount(); j++) {
            if (folder->child(j)->data(GameListItemPath::ProgramIdRole).toULongLong() ==
                program_id) {
                QList<QStandardItem*> list;
                for (int k = 0; k < COLUMN_COUNT; k++) {
                    list.append(folder->child(j, k)->clone());
                }
                list[0]->setData(folder->child(j)->data(GameListItem::SortRole),
                                 GameListItem::SortRole);
                list[0]->setText(folder->child(j)->data(Qt::DisplayRole).toString());

                favorites_row->appendRow(list);
                return;
            }
        }
    }
}

void GameListModel::RemoveFavorite(u64 program_id) {
    auto* favorites_row = item(0);

    for (int i = 0; i < favorites_row->rowCount(); i++) {
        const auto* game = favorites_row->child(i);
        if (game->data(GameListItemPath::ProgramIdRole).toULongLong() == program_id) {
            favorites_row->removeRow(i);
            return;
        }
    }
}

void GameListModel::LoadCompatibilityList() {
    QFile compat_list{QStringLiteral(":compatibility_list/compatibility_list.json")};

    if (!compat_list.open(QFile::ReadOnly | QFile::Text)) {
        LOG_ERROR(Frontend, "Unable to open game compatibility list");
        return;
    }

    if (compat_list.size() == 0) {
        LOG_WARNING(Frontend, "Game compatibility list is empty");
        return;
    }

    const QByteArray content = compat_list.readAll();
    if (content.isEmpty()) {
        LOG_ERROR(Frontend, "Unable to completely read game compatibility list");
        return;
    }

    const QJsonDocument json = QJsonDocument::fromJson(content);
    const QJsonArray arr = json.array();

    for (const QJsonValue& value : arr) {
        const QJsonObject game = value.toObject();
        const QString compatibility_key = QStringLiteral("compatibility");

        if (!game.contains(compatibility_key) || !game[compatibility_key].isDouble()) {
            continue;
        }

        const int compatibility = game[compatibility_key].toInt();
        const QString directory = game[QStringLiteral("directory")].toString();
        const QJsonArray ids = game[QStringLiteral("releases")].toArray();

        for (const QJsonValue& id_ref : ids) {
            const QJsonObject id_object = id_ref.toObject();
            const QString id = id_object[QStringLiteral("id")].toString();

            compatibility_list.emplace(id.toUpper().toStdString(),
                                       std::make_pair(QString::number(compatibility), directory));
        }
    }
}

void GameListModel::Repopulate() {
    current_worker.reset();
    QtCommon::system->GetFileSystemController().CreateFactories(*QtCommon::vfs);
    PopulateAsync(UISettings::values.game_dirs);
}

void GameListModel::RefreshGameDirectory() {
    ResetExternalWatcher();
    if (!UISettings::values.game_dirs.empty() && current_worker != nullptr) {
        LOG_INFO(Frontend, "Change detected in the games directory. Reloading game list.");
        Repopulate();
    }
}

void GameListModel::RefreshExternalContent() {
    if (!UISettings::values.game_dirs.empty() && current_worker != nullptr) {
        LOG_INFO(Frontend, "External content directory changed. Clearing metadata cache.");
        QtCommon::Game::ResetMetadata(false);
        Repopulate();
    }
}

void GameListModel::ResetExternalWatcher() {
    auto watch_dirs = external_watcher->directories();
    if (!watch_dirs.isEmpty()) {
        external_watcher->removePaths(watch_dirs);
    }

    for (const std::string& dir : Settings::values.external_content_dirs) {
        external_watcher->addPath(QString::fromStdString(dir));
    }
}

void GameListModel::RetranslateUI() {
    setHeaderData(COLUMN_NAME, Qt::Horizontal, tr("Name"));
    setHeaderData(COLUMN_COMPATIBILITY, Qt::Horizontal, tr("Compatibility"));
    setHeaderData(COLUMN_ADD_ONS, Qt::Horizontal, tr("Add-ons"));
    setHeaderData(COLUMN_FILE_TYPE, Qt::Horizontal, tr("File type"));
    setHeaderData(COLUMN_SIZE, Qt::Horizontal, tr("Size"));
    setHeaderData(COLUMN_PLAY_TIME, Qt::Horizontal, tr("Play time"));
}

QFileSystemWatcher* GameListModel::GetWatcher() const {
    return watcher;
}

const CompatibilityList& GameListModel::GetCompatibilityList() const {
    return compatibility_list;
}

void GameListModel::SetFlat(bool flat) {
    m_flat = flat;
}
