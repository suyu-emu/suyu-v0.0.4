// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2015 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QFileSystemWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QStandardItemModel>
#include <QString>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include "common/common_types.h"
#include "core/core.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/util/game.h"
#include "yuzu/compatibility_list.h"
#include "frontend_common/play_time_manager.h"

namespace Core {
class System;
}

class ControllerNavigation;
class GameListWorker;
class GameListSearchField;
class GameListDir;
class MainWindow;
enum class AmLaunchType;
enum class StartGameType;

namespace FileSys {
class ManualContentProvider;
class VfsFilesystem;
} // namespace FileSys

enum class GameListOpenTarget {
    SaveData,
    ModData,
};

enum class DumpRomFSTarget {
    Normal,
    SDMC,
};

class GameList : public QWidget {
    Q_OBJECT

public:
    enum {
        COLUMN_NAME,
        COLUMN_FILE_TYPE,
        COLUMN_SIZE,
        COLUMN_PLAY_TIME,
        COLUMN_ADD_ONS,
        COLUMN_COMPATIBILITY,
        COLUMN_COUNT, // Number of columns
    };

    explicit GameList(std::shared_ptr<FileSys::VfsFilesystem> vfs_,
                      FileSys::ManualContentProvider* provider_,
                      PlayTime::PlayTimeManager& play_time_manager_, Core::System& system_,
                      MainWindow* parent = nullptr);
    ~GameList() override;

    QString GetLastFilterResultItem() const;
    void ClearFilter();
    void SetFilterFocus();
    void SetFilterVisible(bool visibility);
    bool IsEmpty() const;

    void LoadCompatibilityList();
    void PopulateAsync(QVector<UISettings::GameDir>& game_dirs);

    void SaveInterfaceLayout();
    void LoadInterfaceLayout();

    QStandardItemModel* GetModel() const;

    /// Disables events from the emulated controller
    void UnloadController();

    static const QStringList supported_file_extensions;

public slots:
    void RefreshGameDirectory();

signals:
    void BootGame(const QString& game_path, StartGameType type);
    void GameChosen(const QString& game_path, const u64 title_id = 0);
    void OpenFolderRequested(u64 program_id, GameListOpenTarget target,
                             const std::string& game_path);
    void OpenTransferableShaderCacheRequested(u64 program_id);
    void RemoveInstalledEntryRequested(u64 program_id, QtCommon::Game::InstalledEntryType type);
    void RemoveFileRequested(u64 program_id, QtCommon::Game::GameListRemoveTarget target,
                             const std::string& game_path);
    void RemovePlayTimeRequested(u64 program_id);
    void SetPlayTimeRequested(u64 program_id);
    void DumpRomFSRequested(u64 program_id, const std::string& game_path, DumpRomFSTarget target);
    void VerifyIntegrityRequested(const std::string& game_path);
    void CopyTIDRequested(u64 program_id);
    void CreateShortcut(u64 program_id, const std::string& game_path,
                        const QtCommon::Game::ShortcutTarget target);
    void NavigateToGamedbEntryRequested(u64 program_id,
                                        const CompatibilityList& compatibility_list);
    void OpenPerGameGeneralRequested(const std::string& file);
    void LinkToRyujinxRequested(const u64 &program_id);
    void OpenDirectory(const QString& directory);
    void AddDirectory();
    void ShowList(bool show);
    void PopulatingCompleted();
    void SaveConfig();

private slots:
    void OnItemExpanded(const QModelIndex& item);
    void OnTextChanged(const QString& new_text);
    void OnFilterCloseClicked();
    void OnUpdateThemedIcons();

private:
    friend class GameListWorker;
    void WorkerEvent();

    void AddDirEntry(GameListDir* entry_items);
    void AddEntry(const QList<QStandardItem*>& entry_items, GameListDir* parent);
    void DonePopulating(const QStringList& watch_list);

private:
    void ValidateEntry(const QModelIndex& item);

    void ToggleFavorite(u64 program_id);
    void AddFavorite(u64 program_id);
    void RemoveFavorite(u64 program_id);

    void PopupContextMenu(const QPoint& menu_location);
    void AddGamePopup(QMenu& context_menu, u64 program_id, const std::string& path);
    void AddCustomDirPopup(QMenu& context_menu, QModelIndex selected);
    void AddPermDirPopup(QMenu& context_menu, QModelIndex selected);
    void AddFavoritesPopup(QMenu& context_menu);

    void changeEvent(QEvent*) override;
    void RetranslateUI();

    std::shared_ptr<FileSys::VfsFilesystem> vfs;
    FileSys::ManualContentProvider* provider;
    GameListSearchField* search_field;
    MainWindow* main_window = nullptr;
    QVBoxLayout* layout = nullptr;
    QTreeView* tree_view = nullptr;
    QStandardItemModel* item_model = nullptr;
    std::unique_ptr<GameListWorker> current_worker;
    QFileSystemWatcher* watcher = nullptr;
    ControllerNavigation* controller_navigation = nullptr;
    CompatibilityList compatibility_list;

    friend class GameListSearchField;

    const PlayTime::PlayTimeManager& play_time_manager;
    Core::System& system;
};

class GameListPlaceholder : public QWidget {
    Q_OBJECT
public:
    explicit GameListPlaceholder(MainWindow* parent = nullptr);
    ~GameListPlaceholder();

signals:
    void AddDirectory();

private slots:
    void onUpdateThemedIcons();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    QVBoxLayout* layout = nullptr;
    QLabel* image = nullptr;
    QLabel* text = nullptr;
};
