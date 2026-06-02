// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QHeaderView>
#include <QScroller>
#include <QScrollerProperties>

#include "qt_common/config/uisettings.h"
#include "qt_common/game_list/game_list_p.h"
#include "yuzu/game/game_tree.h"
#include "qt_common/game_list/model.h"

GameTree::GameTree(QWidget* parent) : QTreeView{parent} {
    setAlternatingRowColors(true);
    setSelectionMode(QHeaderView::SingleSelection);
    setSelectionBehavior(QHeaderView::SelectRows);
    setVerticalScrollMode(QHeaderView::ScrollPerPixel);
    setHorizontalScrollMode(QHeaderView::ScrollPerPixel);
    setSortingEnabled(true);
    setEditTriggers(QHeaderView::NoEditTriggers);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    setStyleSheet(QStringLiteral("QTreeView{ border: none; }"));

    connect(this, &QTreeView::expanded, this, &GameTree::OnItemExpanded);
    connect(this, &QTreeView::collapsed, this, &GameTree::OnItemExpanded);
}

void GameTree::SetModel(GameListModel* model) {
    QTreeView::setModel(model);
    LoadInterfaceLayout();
    UpdateColumnVisibility(model);
}

void GameTree::OnItemExpanded(const QModelIndex& item) {
    const auto type = item.data(GameListItem::TypeRole).value<GameListItemType>();
    const bool is_dir = type == GameListItemType::CustomDir || type == GameListItemType::SdmcDir ||
                        type == GameListItemType::UserNandDir ||
                        type == GameListItemType::SysNandDir;
    const bool is_fave = type == GameListItemType::Favorites;
    if (!is_dir && !is_fave) {
        return;
    }
    const bool is_expanded = isExpanded(item);
    if (is_fave) {
        UISettings::values.favorites_expanded = is_expanded;
        return;
    }
    const int item_dir_index = item.data(GameListDir::GameDirRole).toInt();
    UISettings::values.game_dirs[item_dir_index].expanded = is_expanded;
}

void GameTree::SaveInterfaceLayout() {
    UISettings::values.gamelist_header_state = header()->saveState();
}

void GameTree::LoadInterfaceLayout() {
    auto* hdr = header();

    if (hdr->restoreState(UISettings::values.gamelist_header_state))
        return;

    hdr->resizeSection(GameListModel::COLUMN_NAME, 840);
}

void GameTree::UpdateColumnVisibility(GameListModel* model) {
    Q_UNUSED(model)
    setColumnHidden(GameListModel::COLUMN_ADD_ONS, !UISettings::values.show_add_ons);
    setColumnHidden(GameListModel::COLUMN_COMPATIBILITY, !UISettings::values.show_compat);
    setColumnHidden(GameListModel::COLUMN_FILE_TYPE, !UISettings::values.show_types);
    setColumnHidden(GameListModel::COLUMN_SIZE, !UISettings::values.show_size);
    setColumnHidden(GameListModel::COLUMN_PLAY_TIME, !UISettings::values.show_play_time);
}

QString GameTree::GetLastFilterResultItem() const {
    QString file_path;

    auto* model = qobject_cast<GameListModel*>(QTreeView::model());
    if (!model)
        return {};

    for (int i = 1; i < model->rowCount() - 1; ++i) {
        const QStandardItem* folder = model->item(i, 0);
        const QModelIndex folder_index = folder->index();
        const int children_count = folder->rowCount();

        for (int j = 0; j < children_count; ++j) {
            if (isRowHidden(j, folder_index)) {
                continue;
            }

            const QStandardItem* child = folder->child(j, 0);
            file_path = child->data(GameListItemPath::FullPathRole).toString();
        }
    }

    return file_path;
}

int GameTree::FilterClosedResultCount(GameListModel* model) {
    int children_total = 0;

    auto hide_favorites_row = UISettings::values.favorited_ids.size() == 0;
    setRowHidden(0, model->invisibleRootItem()->index(), hide_favorites_row);

    for (int i = 1; i < model->rowCount() - 1; ++i) {
        auto* folder = model->item(i, 0);
        const QModelIndex folder_index = folder->index();
        const int children_count = folder->rowCount();
        for (int j = 0; j < children_count; ++j) {
            ++children_total;
            setRowHidden(j, folder_index, false);
        }
    }

    return children_total;
}

void GameTree::ApplyFilter(const QString& edit_filter_text, GameListModel* model) {
    int children_total = 0;
    int result_count = 0;

    if (edit_filter_text.isEmpty()) {
        children_total = FilterClosedResultCount(model);
        emit FilterResultReady(children_total, children_total);
        return;
    }

    setRowHidden(0, model->invisibleRootItem()->index(), true);

    for (int i = 1; i < model->rowCount() - 1; ++i) {
        auto* folder = model->item(i, 0);
        const QModelIndex folder_index = folder->index();
        const int children_count = folder->rowCount();

        for (int j = 0; j < children_count; ++j) {
            ++children_total;

            const QStandardItem* child = folder->child(j, 0);

            const auto program_id = child->data(GameListItemPath::ProgramIdRole).toULongLong();

            const QString file_path =
                child->data(GameListItemPath::FullPathRole).toString().toLower();
            const QString file_title =
                child->data(GameListItemPath::TitleRole).toString().toLower();
            const QString file_program_id =
                QStringLiteral("%1").arg(program_id, 16, 16, QLatin1Char{'0'});

            const QString file_name =
                file_path.mid(file_path.lastIndexOf(QLatin1Char{'/'}) + 1) + QLatin1Char{' '} +
                file_title;

            auto ContainsAllWords = [](const QString& haystack, const QString& userinput) {
                const QStringList userinput_split =
                    userinput.split(QLatin1Char{' '}, Qt::SkipEmptyParts);
                return std::all_of(userinput_split.begin(), userinput_split.end(),
                                   [&haystack](const QString& s) { return haystack.contains(s); });
            };

            if (ContainsAllWords(file_name, edit_filter_text) ||
                (file_program_id.size() == 16 && file_program_id.contains(edit_filter_text))) {
                setRowHidden(j, folder_index, false);
                ++result_count;
            } else {
                setRowHidden(j, folder_index, true);
            }
        }
    }

    emit FilterResultReady(result_count, children_total);
}
