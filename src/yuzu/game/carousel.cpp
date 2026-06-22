// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_common/config/uisettings.h"
#include "qt_common/game_list/model.h"
#include "yuzu/game/common.h"
#include "yuzu/game/game_card.h"
#include "yuzu/game/carousel.h"

GameCarousel::GameCarousel(QWidget* parent) : QListView{parent} {
    m_gameCard = new GameCard(this);
    setItemDelegate(m_gameCard);

    setViewMode(QListView::IconMode);
    setMovement(QListView::Static);
    setUniformItemSizes(true);
    setSelectionMode(QAbstractItemView::SingleSelection);

    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setContextMenuPolicy(Qt::CustomContextMenu);

    setSpacing(10);
    setWordWrap(true);
    setTextElideMode(Qt::ElideRight);
    setFlow(QListView::LeftToRight);
    setWrapping(false);
}

void GameCarousel::SetModel(GameListModel* model) {
    QListView::setModel(model);
    UpdateIconSize();
}

void GameCarousel::ApplyFilter(const QString& edit_filter_text, GameListModel* model) {
    int row_count = model->rowCount();

    for (int i = 0; i < row_count; ++i) {
        QStandardItem* item = model->item(i, 0);
        if (!item)
            continue;

        if (Yuzu::FilterMatches(edit_filter_text, item)) {
            setRowHidden(i, false);
        } else {
            setRowHidden(i, true);
        }
    }
}

void GameCarousel::UpdateIconSize() {
    const u32 icon_size = UISettings::values.game_icon_size.GetValue();

    int heightMargin = 0;
    int widthMargin = 80;

    // TODO(crueter): get rid of this nonsense
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

    const int min_item_width = icon_size + widthMargin;
    const int min_item_height = icon_size + heightMargin;
    const int grid_height = std::max(min_item_height, viewport()->height());

    QSize content_size(min_item_width, min_item_height);
    QSize grid_size(min_item_width, grid_height);
    if (gridSize() != grid_size) {
        setUpdatesEnabled(false);

        setGridSize(grid_size);
        m_gameCard->setSize(grid_size, content_size, 0, 0);

        setUpdatesEnabled(true);
    }
}

QModelIndex GameCarousel::indexAt(const QPoint& point) const {
    QModelIndex index = QListView::indexAt(point);
    if (!index.isValid())
        return {};
    if (m_gameCard && !m_gameCard->hitTest(point, index, this, visualRect(index)))
        return {};
    return index;
}
