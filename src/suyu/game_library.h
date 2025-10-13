// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QVector>
#include <QHash>
#include <QMutex>

#include "common/common_types.h"
#include "suyu/game_card.h"
#include "suyu/compatibility_list.h"

class GameLibraryWorker;
class GMainWindow;

namespace FileSys {
class ManualContentProvider;
class VfsFilesystem;
} // namespace FileSys

namespace PlayTime {
class PlayTimeManager;
}

namespace Core {
class System;
}

enum class GameLibraryViewMode {
    Grid,
    List
};

enum class GameLibrarySortMode {
    Title,
    Developer,
    Size,
    PlayTime,
    Compatibility,
    Type,
    DateAdded
};

class GameLibrary : public QWidget {
    Q_OBJECT

public:
    explicit GameLibrary(std::shared_ptr<FileSys::VfsFilesystem> vfs_,
                        FileSys::ManualContentProvider* provider_,
                        PlayTime::PlayTimeManager& play_time_manager_,
                        Core::System& system_,
                        GMainWindow* parent = nullptr);
    ~GameLibrary() override;

    void LoadCompatibilityList();
    void PopulateAsync(QVector<UISettings::GameDir>& game_dirs);
    void ClearList();
    void ClearFilter();
    void SetFilterFocus();
    void SetFilterVisible(bool visibility);
    bool IsEmpty() const;

    void SetViewMode(GameLibraryViewMode mode);
    GameLibraryViewMode GetViewMode() const;

    void SetSortMode(GameLibrarySortMode mode);
    GameLibrarySortMode GetSortMode() const;

    void RefreshGameDirectory();
    void AddGamePopup(QMenu& context_menu, u64 program_id, const std::string& path);
    void AddCustomDirPopup(QMenu& context_menu, QModelIndex selected);
    void AddPermDirPopup(QMenu& context_menu, QModelIndex selected);

    QString GetLastFilterResultItem() const;

signals:
    void GameChosen(const QString& game_path, const u64 title_id);
    void ShouldCancelWorker();
    void OpenFolderRequested(u64 program_id, GameListOpenTarget target, const std::string& game_path);
    void OpenTransferableShaderCacheRequested(u64 program_id);
    void RemoveInstalledEntryRequested(u64 program_id, InstalledEntryType type);
    void RemoveFileRequested(u64 program_id, GameListRemoveTarget target, const std::string& game_path);
    void DumpRomFSRequested(u64 program_id, const std::string& game_path, DumpRomFSTarget target);
    void CopyTIDRequested(u64 program_id);
    void NavigateToGamedbEntryRequested(u64 program_id, const CompatibilityList& compatibility_list);
    void CreateShortcutRequested(u64 program_id, const std::string& game_path, GameListShortcutTarget target);
    void OpenDirectory(const QString& directory);
    void AddDirectory();
    void ShowList(bool show);
    void PopulatingCompleted();

public slots:
    void OnFilterTextChanged(const QString& new_text);
    void OnUpdateThemedIcons();

private slots:
    void OnGameCardSelected(const QString& file_path);
    void OnGameCardDoubleClicked(const QString& file_path);
    void OnGameCardRightClicked(const QString& file_path, const QPoint& global_pos);
    void OnViewModeChanged();
    void OnSortModeChanged();
    void OnSearchTimerTimeout();
    void OnPopulationCompleted();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void SetupUI();
    void SetupToolbar();
    void SetupGameArea();
    void UpdateLayout();
    void ApplyFilter();
    void SortGameCards();
    void AddGameCard(const QString& title, const QString& file_path, const QString& program_id,
                    const QString& developer, u64 program_id_numeric, const QString& version,
                    const QString& type, u64 size, const QString& compatibility,
                    const QPixmap& icon, const QString& play_time);
    void RemoveGameCard(const QString& file_path);
    void ClearGameCards();
    void UpdateGameCardPlayTime(u64 program_id, const QString& play_time);
    void SelectGameCard(GameCard* card);
    void DeselectAllGameCards();
    GameCard* FindGameCard(const QString& file_path) const;
    GameCard* FindGameCardByProgramId(u64 program_id) const;
    void ShowContextMenu(const QString& file_path, const QPoint& global_pos);
    void UpdateSearchResults();
    void AnimateFilterChange();

    // UI Components
    QVBoxLayout* main_layout;
    QHBoxLayout* toolbar_layout;
    QLineEdit* search_bar;
    QPushButton* view_mode_button;
    QComboBox* sort_combo;
    QLabel* game_count_label;
    QScrollArea* scroll_area;
    QWidget* game_container;
    GameCardLayout* game_layout;

    // Animation
    QPropertyAnimation* filter_animation;
    QGraphicsOpacityEffect* opacity_effect;

    // Data
    QVector<GameCard*> game_cards;
    QVector<GameCard*> filtered_cards;
    QHash<QString, GameCard*> file_path_to_card;
    QHash<u64, GameCard*> program_id_to_card;
    GameCard* selected_card;
    QString current_filter;
    GameLibraryViewMode view_mode;
    GameLibrarySortMode sort_mode;
    QTimer* search_timer;

    // Worker thread
    GameLibraryWorker* worker;
    QThread* worker_thread;

    // Dependencies
    std::shared_ptr<FileSys::VfsFilesystem> vfs;
    FileSys::ManualContentProvider* provider;
    PlayTime::PlayTimeManager& play_time_manager;
    Core::System& system;
    GMainWindow* main_window;
    CompatibilityList compatibility_list;

    // Thread safety
    QMutex cards_mutex;

    // Constants
    static constexpr int SEARCH_DELAY_MS = 300;
    static constexpr int ANIMATION_DURATION_MS = 200;
};

class GameLibraryWorker : public QObject {
    Q_OBJECT

public:
    explicit GameLibraryWorker(std::shared_ptr<FileSys::VfsFilesystem> vfs_,
                              FileSys::ManualContentProvider* provider_,
                              PlayTime::PlayTimeManager& play_time_manager_,
                              Core::System& system_);
    ~GameLibraryWorker() override;

public slots:
    void AddInstalledTitlesToGameList();
    void FillControllerList(const QVector<UISettings::GameDir>& game_dirs);

signals:
    void EntryReady(const QString& title, const QString& file_path, const QString& program_id,
                   const QString& developer, u64 program_id_numeric, const QString& version,
                   const QString& type, u64 size, const QString& compatibility,
                   const QPixmap& icon, const QString& play_time);
    void Finished();
    void DirEntryReady(GameListDir* parent_dir, const QVector<QStandardItem*>& entry_items);

private:
    void ScanFileSystem(QVector<UISettings::GameDir>& game_dirs);
    void ProcessFile(const QString& file_path);
    QPixmap GetGameIcon(u64 program_id, const QString& file_path);
    QString GetCompatibilityRating(u64 program_id);

    std::shared_ptr<FileSys::VfsFilesystem> vfs;
    FileSys::ManualContentProvider* provider;
    PlayTime::PlayTimeManager& play_time_manager;
    Core::System& system;
    CompatibilityList compatibility_list;
    std::atomic_bool stop_processing{false};
};