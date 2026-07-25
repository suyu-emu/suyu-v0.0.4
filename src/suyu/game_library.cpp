// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suyu/game_library.h"
#include "suyu/nintendo_account.h"

#include <QApplication>
#include <QPainter>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QResizeEvent>
#include <QStandardItemModel>
#include <QThread>
#include <QTimer>
#include <QToolTip>
#include <QUrl>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QFile>
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <QRegularExpression>

#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/submission_package.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/loader.h"
#include "suyu/game_list_p.h"
#include "suyu/main.h"
#include "suyu/uisettings.h"

GameLibrary::GameLibrary(std::shared_ptr<FileSys::VfsFilesystem> vfs_,
                        FileSys::ManualContentProvider* provider_,
                        PlayTime::PlayTimeManager& play_time_manager_,
                        Core::System& system_,
                        GMainWindow* parent)
    : QWidget(parent), main_layout(nullptr), toolbar_layout(nullptr), search_bar(nullptr),
      view_mode_button(nullptr), sort_combo(nullptr), game_count_label(nullptr),
      scroll_area(nullptr), game_container(nullptr), game_layout(nullptr),
      filter_animation(nullptr), opacity_effect(nullptr), selected_card(nullptr),
      view_mode(GameLibraryViewMode::Grid), sort_mode(GameLibrarySortMode::Title),
      search_timer(nullptr), worker(nullptr), worker_thread(nullptr), vfs(std::move(vfs_)),
      provider(provider_), play_time_manager(play_time_manager_), system(system_),
      main_window(parent) {

    setObjectName(QStringLiteral("gameLibraryContainer"));
    SetupUI();

    // Initialize worker thread
    worker = new GameLibraryWorker(vfs, provider, play_time_manager, system);
    worker_thread = new QThread(this);
    worker->moveToThread(worker_thread);

    // Drive the real scan when the thread starts. This used to run
    // AddInstalledTitlesToGameList (an empty stub that emitted Finished()
    // immediately) while the actual scan was hooked up in PopulateAsync via a
    // connect() whose "signal" was really just a slot, so nothing ever emitted
    // it - between the two, the library never scanned anything at all.
    connect(worker_thread, &QThread::started, worker, [this]() {
        worker->FillControllerList(pending_game_dirs);
    });
    connect(worker, &GameLibraryWorker::EntryReady, this,
            [this](const QString& title, const QString& file_path, const QString& program_id,
                   const QString& developer, u64 program_id_numeric, const QString& version,
                   const QString& type, u64 size, const QString& compatibility,
                   const QPixmap& icon, const QString& play_time) {
                AddGameCard(title, file_path, program_id, developer, program_id_numeric,
                           version, type, size, compatibility, icon, play_time);
            });
    connect(worker, &GameLibraryWorker::Finished, this, &GameLibrary::OnPopulationCompleted);
    connect(this, &GameLibrary::ShouldCancelWorker, worker, [this]() {
        if (worker) {
            worker->RequestStop();
        }
    });
    connect(worker_thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker_thread, &QThread::finished, worker_thread, &QObject::deleteLater);

    // Setup search timer
    search_timer = new QTimer(this);
    search_timer->setSingleShot(true);
    search_timer->setInterval(SEARCH_DELAY_MS);
    connect(search_timer, &QTimer::timeout, this, &GameLibrary::OnSearchTimerTimeout);
}

GameLibrary::~GameLibrary() {
    if (worker_thread && worker_thread->isRunning()) {
        emit ShouldCancelWorker();
        worker_thread->quit();
        worker_thread->wait(5000);
    }
}

void GameLibrary::SetupUI() {
    main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(16, 16, 16, 16);
    main_layout->setSpacing(12);

    SetupToolbar();
    SetupGameArea();

    // Setup animations
    opacity_effect = new QGraphicsOpacityEffect(this);
    opacity_effect->setOpacity(1.0);
    game_container->setGraphicsEffect(opacity_effect);

    filter_animation = new QPropertyAnimation(opacity_effect, "opacity", this);
    filter_animation->setDuration(ANIMATION_DURATION_MS);
}

void GameLibrary::SetupToolbar() {
    toolbar_layout = new QHBoxLayout();
    toolbar_layout->setSpacing(12);

    // Search bar
    search_bar = new QLineEdit();
    search_bar->setPlaceholderText(QStringLiteral("Search games..."));
    search_bar->setMinimumWidth(300);
    search_bar->setMaximumWidth(500);
    connect(search_bar, &QLineEdit::textChanged, this, &GameLibrary::OnFilterTextChanged);
    toolbar_layout->addWidget(search_bar);

    toolbar_layout->addStretch();

    // Game count label
    game_count_label = new QLabel(QStringLiteral("0 games"));
    game_count_label->setObjectName(QStringLiteral("subtitle"));
    toolbar_layout->addWidget(game_count_label);

    // Sort combo box
    sort_combo = new QComboBox();
    sort_combo->addItem(QStringLiteral("Sort by Title"), static_cast<int>(GameLibrarySortMode::Title));
    sort_combo->addItem(QStringLiteral("Sort by Developer"), static_cast<int>(GameLibrarySortMode::Developer));
    sort_combo->addItem(QStringLiteral("Sort by Size"), static_cast<int>(GameLibrarySortMode::Size));
    sort_combo->addItem(QStringLiteral("Sort by Play Time"), static_cast<int>(GameLibrarySortMode::PlayTime));
    sort_combo->addItem(QStringLiteral("Sort by Compatibility"), static_cast<int>(GameLibrarySortMode::Compatibility));
    sort_combo->addItem(QStringLiteral("Sort by Type"), static_cast<int>(GameLibrarySortMode::Type));
    connect(sort_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GameLibrary::OnSortModeChanged);
    toolbar_layout->addWidget(sort_combo);

    // View mode button
    view_mode_button = new QPushButton();
    view_mode_button->setIcon(QIcon(QStringLiteral(":/icons/grid_view.svg")));
    view_mode_button->setToolTip(QStringLiteral("Switch to List View"));
    view_mode_button->setFixedSize(32, 32);
    connect(view_mode_button, &QPushButton::clicked, this, &GameLibrary::OnViewModeChanged);
    toolbar_layout->addWidget(view_mode_button);

    main_layout->addLayout(toolbar_layout);
}

void GameLibrary::SetupGameArea() {
    // Create scroll area
    scroll_area = new QScrollArea();
    scroll_area->setWidgetResizable(true);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_area->setFrameShape(QFrame::NoFrame);

    // Create game container
    game_container = new QWidget();
    game_container->setObjectName(QStringLiteral("gameContainer"));

    // Create game layout
    game_layout = new GameCardLayout(game_container);
    game_layout->setSpacing(16);
    game_container->setLayout(game_layout);

    scroll_area->setWidget(game_container);
    main_layout->addWidget(scroll_area);
}

void GameLibrary::LoadCompatibilityList() {
    // CompatibilityList is loaded by the worker thread
    compatibility_list.clear();
}

void GameLibrary::PopulateAsync(QVector<UISettings::GameDir>& game_dirs) {
    if (worker_thread && !worker_thread->isRunning()) {
        // Clear existing games
        ClearGameCards();

        // Hand the directories to the worker before starting it; the thread's
        // started() handler reads them from here.
        pending_game_dirs = game_dirs;
        worker_thread->start();
    }
}

void GameLibrary::ClearList() {
    ClearGameCards();
}

void GameLibrary::ClearFilter() {
    search_bar->clear();
    current_filter.clear();
    ApplyFilter();
}

void GameLibrary::SetFilterFocus() {
    search_bar->setFocus();
}

void GameLibrary::SetFilterVisible(bool visibility) {
    search_bar->setVisible(visibility);
}

bool GameLibrary::IsEmpty() const {
    return game_cards.isEmpty();
}

void GameLibrary::SetViewMode(GameLibraryViewMode mode) {
    if (view_mode != mode) {
        view_mode = mode;
        UpdateLayout();

        // Update button icon and tooltip
        if (mode == GameLibraryViewMode::Grid) {
            view_mode_button->setIcon(QIcon(QStringLiteral(":/icons/list_view.svg")));
            view_mode_button->setToolTip(QStringLiteral("Switch to List View"));
        } else {
            view_mode_button->setIcon(QIcon(QStringLiteral(":/icons/grid_view.svg")));
            view_mode_button->setToolTip(QStringLiteral("Switch to Grid View"));
        }
    }
}

GameLibraryViewMode GameLibrary::GetViewMode() const {
    return view_mode;
}

void GameLibrary::SetSortMode(GameLibrarySortMode mode) {
    if (sort_mode != mode) {
        sort_mode = mode;
        sort_combo->setCurrentIndex(static_cast<int>(mode));
        SortGameCards();
    }
}

GameLibrarySortMode GameLibrary::GetSortMode() const {
    return sort_mode;
}

void GameLibrary::RefreshGameDirectory() {
    // Emit signal to refresh
    emit PopulatingCompleted();
}

QString GameLibrary::GetLastFilterResultItem() const {
    if (!filtered_cards.isEmpty()) {
        return filtered_cards.last()->GetFilePath();
    }
    return QString();
}

void GameLibrary::OnFilterTextChanged(const QString& new_text) {
    current_filter = new_text;
    search_timer->start(); // Restart timer
}

void GameLibrary::OnUpdateThemedIcons() {
    // Update icons for current theme
    view_mode_button->setIcon(view_mode == GameLibraryViewMode::Grid ?
                             QIcon(QStringLiteral(":/icons/list_view.svg")) : QIcon(QStringLiteral(":/icons/grid_view.svg")));
}

void GameLibrary::OnGameCardSelected(const QString& file_path) {
    GameCard* card = FindGameCard(file_path);
    if (card) {
        SelectGameCard(card);
    }
}

void GameLibrary::OnGameCardDoubleClicked(const QString& file_path) {
    GameCard* card = FindGameCard(file_path);
    if (card) {
        emit GameChosen(file_path, card->GetProgramIdNumeric());
    }
}

void GameLibrary::OnGameCardRightClicked(const QString& file_path, const QPoint& global_pos) {
    ShowContextMenu(file_path, global_pos);
}

void GameLibrary::OnViewModeChanged() {
    GameLibraryViewMode new_mode = (view_mode == GameLibraryViewMode::Grid) ?
                                   GameLibraryViewMode::List : GameLibraryViewMode::Grid;
    SetViewMode(new_mode);
}

void GameLibrary::OnSortModeChanged() {
    int index = sort_combo->currentIndex();
    GameLibrarySortMode new_mode = static_cast<GameLibrarySortMode>(
        sort_combo->itemData(index).toInt());
    SetSortMode(new_mode);
}

void GameLibrary::OnSearchTimerTimeout() {
    ApplyFilter();
}

void GameLibrary::OnPopulationCompleted() {
    emit PopulatingCompleted();

    // Update game count
    int visible_count = filtered_cards.size();
    int total_count = game_cards.size();

    if (current_filter.isEmpty()) {
        game_count_label->setText(QStringLiteral("%1 games").arg(total_count));
    } else {
        game_count_label->setText(QStringLiteral("%1 of %2 games").arg(visible_count).arg(total_count));
    }
}

void GameLibrary::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (selected_card) {
            emit GameChosen(selected_card->GetFilePath(), selected_card->GetProgramIdNumeric());
        }
    } else if (event->key() == Qt::Key_Delete) {
        if (selected_card) {
            // Handle delete key - could show remove dialog
        }
    }
    QWidget::keyPressEvent(event);
}

void GameLibrary::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    UpdateLayout();
}

void GameLibrary::UpdateLayout() {
    if (!game_layout) return;

    // Force layout update
    game_layout->invalidate();
    game_container->updateGeometry();
    scroll_area->updateGeometry();
}

void GameLibrary::ApplyFilter() {
    QMutexLocker locker(&cards_mutex);

    filtered_cards.clear();

    if (current_filter.isEmpty()) {
        filtered_cards = game_cards;
    } else {
        QString filter_lower = current_filter.toLower();
        for (GameCard* card : game_cards) {
            if (card->GetTitle().toLower().contains(filter_lower)) {
                filtered_cards.append(card);
            }
        }
    }

    // Hide/show cards based on filter
    for (GameCard* card : game_cards) {
        card->setVisible(filtered_cards.contains(card));
    }

    SortGameCards();
    UpdateLayout();

    // Update game count
    OnPopulationCompleted();
}

void GameLibrary::SortGameCards() {
    if (filtered_cards.isEmpty()) return;

    std::sort(filtered_cards.begin(), filtered_cards.end(),
              [this](const GameCard* a, const GameCard* b) {
        switch (sort_mode) {
            case GameLibrarySortMode::Title:
                return a->GetTitle().toLower() < b->GetTitle().toLower();
            case GameLibrarySortMode::Developer:
                // Would need to store developer info in GameCard
                return a->GetTitle().toLower() < b->GetTitle().toLower();
            case GameLibrarySortMode::Size:
                // Would need to store size info in GameCard
                return a->GetTitle().toLower() < b->GetTitle().toLower();
            case GameLibrarySortMode::PlayTime:
                // Would need to store play time info in GameCard
                return a->GetTitle().toLower() < b->GetTitle().toLower();
            case GameLibrarySortMode::Compatibility:
                // Would need to store compatibility info in GameCard
                return a->GetTitle().toLower() < b->GetTitle().toLower();
            case GameLibrarySortMode::Type:
                // Would need to store type info in GameCard
                return a->GetTitle().toLower() < b->GetTitle().toLower();
            case GameLibrarySortMode::DateAdded:
                // Would need to store date added info in GameCard
                return a->GetTitle().toLower() < b->GetTitle().toLower();
            default:
                return a->GetTitle().toLower() < b->GetTitle().toLower();
        }
    });

    // Reorder cards in layout
    for (int i = 0; i < filtered_cards.size(); ++i) {
        GameCard* card = filtered_cards[i];
        game_layout->removeWidget(card);
        game_layout->addWidget(card);
    }
}

void GameLibrary::AddGameCard(const QString& title, const QString& file_path, const QString& program_id,
                             const QString& developer, u64 program_id_numeric, const QString& version,
                             const QString& type, u64 size, const QString& compatibility,
                             const QPixmap& icon, const QString& play_time) {
    QMutexLocker locker(&cards_mutex);

    // Check if card already exists
    if (file_path_to_card.contains(file_path)) {
        return;
    }

    GameCard* card = new GameCard(game_container);
    card->SetGameInfo(title, file_path, program_id, developer, program_id_numeric,
                     version, type, size, compatibility, icon);
    card->SetPlayTime(play_time);

    // Connect signals
    connect(card, &GameCard::GameSelected, this, &GameLibrary::OnGameCardSelected);
    connect(card, &GameCard::GameDoubleClicked, this, &GameLibrary::OnGameCardDoubleClicked);
    connect(card, &GameCard::GameRightClicked, this, &GameLibrary::OnGameCardRightClicked);

    // Add to collections
    game_cards.append(card);
    file_path_to_card[file_path] = card;
    program_id_to_card[program_id_numeric] = card;

    // Add to layout
    game_layout->addWidget(card);

    // Apply current filter
    if (!current_filter.isEmpty()) {
        QString filter_lower = current_filter.toLower();
        card->setVisible(title.toLower().contains(filter_lower));
        if (card->isVisible()) {
            filtered_cards.append(card);
        }
    } else {
        filtered_cards.append(card);
    }
}

void GameLibrary::RemoveGameCard(const QString& file_path) {
    QMutexLocker locker(&cards_mutex);

    GameCard* card = file_path_to_card.value(file_path, nullptr);
    if (!card) return;

    // Remove from collections
    game_cards.removeAll(card);
    filtered_cards.removeAll(card);
    file_path_to_card.remove(file_path);
    program_id_to_card.remove(card->GetProgramIdNumeric());

    // Remove from layout and delete
    game_layout->removeWidget(card);
    card->deleteLater();

    if (selected_card == card) {
        selected_card = nullptr;
    }
}

void GameLibrary::ClearGameCards() {
    QMutexLocker locker(&cards_mutex);

    // Clear collections
    for (GameCard* card : game_cards) {
        game_layout->removeWidget(card);
        card->deleteLater();
    }

    game_cards.clear();
    filtered_cards.clear();
    file_path_to_card.clear();
    program_id_to_card.clear();
    selected_card = nullptr;
}

void GameLibrary::UpdateGameCardPlayTime(u64 program_id, const QString& play_time) {
    GameCard* card = program_id_to_card.value(program_id, nullptr);
    if (card) {
        card->SetPlayTime(play_time);
    }
}

void GameLibrary::SelectGameCard(GameCard* card) {
    if (selected_card == card) return;

    // Deselect previous card
    if (selected_card) {
        selected_card->SetSelected(false);
    }

    // Select new card
    selected_card = card;
    if (selected_card) {
        selected_card->SetSelected(true);
        selected_card->setFocus();
    }
}

void GameLibrary::DeselectAllGameCards() {
    if (selected_card) {
        selected_card->SetSelected(false);
        selected_card = nullptr;
    }
}

GameCard* GameLibrary::FindGameCard(const QString& file_path) const {
    return file_path_to_card.value(file_path, nullptr);
}

GameCard* GameLibrary::FindGameCardByProgramId(u64 program_id) const {
    return program_id_to_card.value(program_id, nullptr);
}

void GameLibrary::ShowContextMenu(const QString& file_path, const QPoint& global_pos) {
    GameCard* card = FindGameCard(file_path);
    if (!card) return;

    QMenu context_menu(this);

    // Add standard game actions
    QAction* play_action = context_menu.addAction(QStringLiteral("Play"));
    connect(play_action, &QAction::triggered, [this, file_path, card]() {
        emit GameChosen(file_path, card->GetProgramIdNumeric());
    });

    context_menu.addSeparator();

    QAction* open_folder_action = context_menu.addAction(QStringLiteral("Open Game Folder"));
    connect(open_folder_action, &QAction::triggered, [this, file_path]() {
        QFileInfo file_info(file_path);
        emit OpenDirectory(file_info.absolutePath());
    });

    QAction* properties_action = context_menu.addAction(QStringLiteral("Properties"));
    connect(properties_action, &QAction::triggered, [card]() {
        // Could open a properties dialog
        Q_UNUSED(card);
    });

    context_menu.exec(global_pos);
}

void GameLibrary::UpdateSearchResults() {
    ApplyFilter();
}

void GameLibrary::AnimateFilterChange() {
    if (!filter_animation || !opacity_effect) return;

    filter_animation->setStartValue(opacity_effect->opacity());
    filter_animation->setEndValue(0.0);
    filter_animation->start();

    connect(filter_animation, &QPropertyAnimation::finished, this, [this]() {
        ApplyFilter();
        filter_animation->setStartValue(0.0);
        filter_animation->setEndValue(1.0);
        filter_animation->start();
    });
}

void GameLibrary::AddGamePopup(QMenu& context_menu, u64 program_id, const std::string& path) {
    // Implementation would depend on existing game list functionality
}

void GameLibrary::AddCustomDirPopup(QMenu& context_menu, QModelIndex selected) {
    // Implementation would depend on existing game list functionality
}

void GameLibrary::AddPermDirPopup(QMenu& context_menu, QModelIndex selected) {
    // Implementation would depend on existing game list functionality
}

// GameLibraryWorker implementation
GameLibraryWorker::GameLibraryWorker(std::shared_ptr<FileSys::VfsFilesystem> vfs_,
                                    FileSys::ManualContentProvider* provider_,
                                    PlayTime::PlayTimeManager& play_time_manager_,
                                    Core::System& system_)
    : vfs(std::move(vfs_)), provider(provider_), play_time_manager(play_time_manager_),
      system(system_) {
}

GameLibraryWorker::~GameLibraryWorker() = default;

void GameLibraryWorker::FillControllerList(const QVector<UISettings::GameDir>& game_dirs) {
    if (stop_processing) return;

    ScanFileSystem(const_cast<QVector<UISettings::GameDir>&>(game_dirs));

    // Merge Nintendo-account titles the user doesn't already have a local dump
    // of. Entries that DO match a local file are skipped outright - the local
    // one is strictly better (it's launchable and carries its own icon).
    const auto owned = LoadNintendoOwnedLibrary();
    for (const auto& game : owned) {
        if (stop_processing) break;
        if (game.title.isEmpty()) continue;

        const u64 pid = game.title_id.toULongLong(nullptr, 16);
        if (pid != 0 && local_program_ids_.count(pid) > 0) {
            continue;
        }

        // Real cover art from Nintendo when the sync supplied a URL; only fall
        // back to a drawn tile when that genuinely isn't available.
        QPixmap icon = FetchRemoteIcon(game.icon_url, game.title_id);
        if (icon.isNull()) {
            icon = QPixmap(256, 256);
            icon.fill(QColor(60, 60, 80));
            QPainter painter(&icon);
            painter.setPen(Qt::white);
            painter.setFont(QFont(QStringLiteral("Segoe UI"), 14));
            painter.drawText(icon.rect(), Qt::AlignCenter | Qt::TextWordWrap, game.title);
            painter.end();
        }

        const QString display_type =
            game.is_digital ? QStringLiteral("eShop") : QStringLiteral("Physical");
        emit EntryReady(game.title, QStringLiteral("nintendo://%1").arg(game.title_id),
                       game.title_id, game.platform, pid, QStringLiteral(""),
                       display_type, 0, QStringLiteral("Unknown"), icon,
                       QStringLiteral("0h 0m"));
    }

    emit Finished();
}

void GameLibraryWorker::ScanFileSystem(QVector<UISettings::GameDir>& game_dirs) {
    for (const auto& game_dir : game_dirs) {
        if (stop_processing) break;

        const QString base_path = QString::fromStdString(game_dir.path);
        QDir dir(base_path);
        if (!dir.exists()) continue;

        QDirIterator it(base_path,
                        QStringList{QStringLiteral("*.nsp"), QStringLiteral("*.xci"),
                                    QStringLiteral("*.nro")},
                        QDir::Files | QDir::Readable,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (stop_processing) break;
            const QString file_path = it.next();
            if (file_path.contains(QStringLiteral(".cnmt.nca"), Qt::CaseInsensitive)) {
                continue;
            }
            ProcessFile(file_path);
        }
    }
}

void GameLibraryWorker::ProcessFile(const QString& file_path) {
    if (stop_processing) return;

    const auto path = file_path.toStdString();
    const auto file = vfs->OpenFile(path, FileSys::OpenMode::Read);
    if (!file) {
        LOG_WARNING(Frontend, "GameLibrary: could not open '{}'", path);
        return;
    }

    auto loader = Loader::GetLoader(system, file);
    if (!loader) {
        LOG_WARNING(Frontend, "GameLibrary: no loader for '{}'", path);
        return;
    }

    const auto file_type = loader->GetFileType();
    if (file_type == Loader::FileType::Unknown || file_type == Loader::FileType::Error) {
        return;
    }

    if (file_type == Loader::FileType::NCA) {
        const auto nca_type = FileSys::NCA{file}.GetType();
        if (nca_type != FileSys::NCAContentType::Program) {
            return;
        }
    }

    // Extract program ID
    u64 program_id = 0;
    loader->ReadProgramId(program_id);

    // Extract title
    std::string raw_title;
    loader->ReadTitle(raw_title);
    QString title = raw_title.empty() ? QFileInfo(file_path).baseName()
                                      : QString::fromStdString(raw_title);
    if (title.trimmed().isEmpty()) {
        return;
    }

    static const QRegularExpression hash_like_title(
        QStringLiteral("^[0-9a-fA-F]{16,}$"));
    if (hash_like_title.match(title.trimmed()).hasMatch()) {
        return;
    }

    // Extract developer and version from NACP control data
    QString developer = QStringLiteral("Unknown");
    QString version = QStringLiteral("1.0.0");
    FileSys::NACP nacp;
    if (loader->ReadControlData(nacp) == Loader::ResultStatus::Success) {
        const auto dev_name = nacp.GetDeveloperName();
        if (!dev_name.empty()) {
            developer = QString::fromStdString(dev_name);
        }
        const auto ver_str = nacp.GetVersionString();
        if (!ver_str.empty()) {
            version = QString::fromStdString(ver_str);
        }
    }

    // Prefer the high-resolution cover the gamer view fetches and caches over
    // the icon embedded in the ROM. The NACP icon is only 256x256, which is
    // smaller than the card renders at on a HiDPI display, so using it here
    // was why the same game looked softer in this view than elsewhere.
    QPixmap icon = LoadCachedCoverArt(title);
    if (icon.isNull()) {
        icon = GetGameIcon(program_id, file_path);
    }

    QString type = QFileInfo(file_path).suffix().toUpper();
    u64 size = QFileInfo(file_path).size();
    QString compatibility = GetCompatibilityRating(program_id);

    // Play time
    QString play_time = QStringLiteral("0h 0m");
    const auto play_seconds =
        play_time_manager.GetPlayTime(program_id);
    if (play_seconds > 0) {
        const auto hours = play_seconds / 3600;
        const auto minutes = (play_seconds % 3600) / 60;
        play_time = QStringLiteral("%1h %2m").arg(hours).arg(minutes);
    }

    local_program_ids_.insert(program_id);
    emit EntryReady(title, file_path, QString::number(program_id, 16), developer,
                   program_id, version, type, size, compatibility, icon, play_time);
}

QPixmap GameLibraryWorker::LoadCachedCoverArt(const QString& title) {
    // Must stay in sync with GamerEnvironment::CoverCachePathForTitle - both
    // views deliberately share one cover cache so art fetched in either shows
    // up in the other.
    if (title.trimmed().isEmpty()) {
        return {};
    }
    const QString cache_root =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
        QStringLiteral("/cover_cache");
    const QByteArray key =
        QCryptographicHash::hash(title.trimmed().toUtf8(), QCryptographicHash::Sha1).toHex();
    const QString path = QDir(cache_root).filePath(QString::fromLatin1(key) +
                                                   QStringLiteral(".png"));
    if (!QFileInfo::exists(path)) {
        return {};
    }
    QPixmap px(path);
    return px;
}

QPixmap GameLibraryWorker::FetchRemoteIcon(const QString& url, const QString& title_id) {
    if (url.isEmpty()) {
        return {};
    }

    // Disk cache first - these are stable Nintendo CDN assets, so re-fetching
    // them on every library scan would be pure waste (and the scan runs on a
    // worker thread where a stall is user-visible).
    const QString cache_dir =
        QString::fromStdString(
            Common::FS::GetSuyuPath(Common::FS::SuyuPath::CacheDir).generic_string()) +
        QStringLiteral("/nintendo_art");
    QDir().mkpath(cache_dir);
    const QString cache_path = QStringLiteral("%1/%2.png").arg(cache_dir, title_id);

    QPixmap cached;
    if (QFile::exists(cache_path) && cached.load(cache_path) && !cached.isNull()) {
        return cached;
    }

    // Worker thread: drive the request with a local event loop plus a hard
    // timeout so a hanging CDN can't wedge the whole library scan.
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeout.start(8000);
    loop.exec();

    QPixmap result;
    if (reply->isFinished() && reply->error() == QNetworkReply::NoError) {
        const QByteArray data = reply->readAll();
        if (result.loadFromData(data) && !result.isNull()) {
            result.save(cache_path, "PNG");
        }
    }
    reply->abort();
    reply->deleteLater();
    return result;
}

QPixmap GameLibraryWorker::GetGameIcon(u64 program_id, const QString& file_path) {
    const auto path = file_path.toStdString();
    const auto file = vfs->OpenFile(path, FileSys::OpenMode::Read);
    if (!file) {
        return QPixmap(QStringLiteral(":/icons/game_placeholder.svg"));
    }

    auto loader = Loader::GetLoader(system, file);
    if (!loader) {
        return QPixmap(QStringLiteral(":/icons/game_placeholder.svg"));
    }

    std::vector<u8> icon_data;
    if (loader->ReadIcon(icon_data) == Loader::ResultStatus::Success && !icon_data.empty()) {
        QPixmap pixmap;
        if (pixmap.loadFromData(icon_data.data(), static_cast<uint>(icon_data.size()))) {
            return pixmap;
        }
    }
    return QPixmap(QStringLiteral(":/icons/game_placeholder.svg"));
}

QString GameLibraryWorker::GetCompatibilityRating(u64 program_id) {
    auto it = FindMatchingCompatibilityEntry(compatibility_list, program_id);
    if (it != compatibility_list.end()) {
        return it->second.first;
    }
    return QStringLiteral("Unknown");
}

#include "game_library.moc"