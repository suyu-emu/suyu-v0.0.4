// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QScroller>
#include <QScrollerProperties>

#include "qt_common/config/uisettings.h"
#include "yuzu/game/game_card.h"
#include "yuzu/game/game_grid.h"
#include "qt_common/game_list/game_list_p.h"
#include "qt_common/game_list/model.h"

GameGrid::GameGrid(QWidget* parent) : QListView{parent} {
    m_gameCard = new GameCard(this);
    setItemDelegate(m_gameCard);

    setViewMode(QListView::ListMode);
    setResizeMode(QListView::Fixed);
    setUniformItemSizes(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setGridSize(QSize(140, 160));
    m_gameCard->setSize(gridSize(), 0, 4);

    setSpacing(10);
    setWordWrap(true);
    setTextElideMode(Qt::ElideRight);
    setFlow(QListView::LeftToRight);
    setWrapping(true);
}

void GameGrid::SetModel(GameListModel* model) {
    QListView::setModel(model);
    UpdateIconSize();
}

void GameGrid::ApplyFilter(const QString& edit_filter_text, GameListModel* model) {
    int row_count = model->rowCount();

    auto ContainsAllWords = [](const QString& haystack, const QString& userinput) {
        const QStringList userinput_split =
            userinput.split(QLatin1Char{' '}, Qt::SkipEmptyParts);
        return std::all_of(userinput_split.begin(), userinput_split.end(),
                           [&haystack](const QString& s) { return haystack.contains(s); });
    };

    for (int i = 0; i < row_count; ++i) {
        QStandardItem* item = model->item(i, 0);
        if (!item)
            continue;

        const QString file_path =
            item->data(GameListItemPath::FullPathRole).toString().toLower();
        const QString file_title =
            item->data(GameListItemPath::TitleRole).toString().toLower();
        const QString file_name = file_path.mid(file_path.lastIndexOf(QLatin1Char{'/'}) + 1) +
                                  QLatin1Char{' '} + file_title;

        if (edit_filter_text.isEmpty() || ContainsAllWords(file_name, edit_filter_text)) {
            setRowHidden(i, false);
        } else {
            setRowHidden(i, true);
        }
    }
}

void GameGrid::UpdateIconSize() {
    const u32 icon_size = UISettings::values.game_icon_size.GetValue();

    int heightMargin = 0;
    int widthMargin = 80;

    if (UISettings::values.show_game_name) {
        switch (icon_size) {
        case 128:
            heightMargin = 65;
            break;
        case 0:
            widthMargin = 120;
            heightMargin = 120;
            break;
        case 64:
            heightMargin = 77;
            break;
        case 32:
        case 256:
            heightMargin = 81;
            break;
        }
    } else {
        widthMargin = 24;
        heightMargin = 24;
    }

    const int view_width = viewport()->width();

    const double spacing = 0.01;
    const int min_item_width = icon_size + widthMargin;

    int columns = std::max(1, (view_width - 16) / min_item_width);
    int stretched_width = ((view_width) - (spacing * (columns - 1))) / columns;

    QSize grid_size(stretched_width, icon_size + heightMargin);
    if (gridSize() != grid_size) {
        setUpdatesEnabled(false);

        setGridSize(grid_size);
        m_gameCard->setSize(grid_size, stretched_width - min_item_width, columns);

        setUpdatesEnabled(true);
    }
}
