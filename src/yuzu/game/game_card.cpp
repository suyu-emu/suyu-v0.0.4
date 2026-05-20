// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QPainter>
#include <QPainterPath>
#include "game_card.h"
#include "qt_common/config/uisettings.h"

GameCard::GameCard(QObject* parent) : QStyledItemDelegate{parent} {
    setObjectName("GameCard");
}

void GameCard::paint(QPainter* painter, const QStyleOptionViewItem& option,
                     const QModelIndex& index) const {
    if (!index.isValid())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Padding, dimensions, alignment...
    const int column = index.row() % m_columns;
    const int cell_width = option.rect.width();
    const int fixed_card_width = cell_width - m_padding;
    const int margins = 8;

    // The gist of it is that this anchors the left and right sides to the edges,
    // while maintaining an even gap between each card.
    // I just smashed random keys into my keyboard until something worked.
    // Don't even bother trying to figure out what the hell this is doing.
    const auto total_row_width = m_columns * cell_width;
    const auto total_gap_space = total_row_width - (margins * 2) - (m_columns * fixed_card_width);
    const auto gap = (m_columns > 1) ? (total_gap_space / (m_columns - 1)) : 0;

    const auto relative_x = margins + (column * (fixed_card_width + gap));
    const auto x_pos = option.rect.left() - (column * cell_width) + static_cast<int>(relative_x);

    // also, add some additional padding here to prevent card overlap
    QRect cardRect(x_pos + 4, option.rect.top() + 4, fixed_card_width - 8,
                   option.rect.height() - margins);

    // colors
    QPalette palette = option.palette;
    QColor backgroundColor = palette.window().color();
    QColor borderColor = palette.dark().color();
    QColor textColor = palette.text().color();

    // if it's selected add a blue background
    if (option.state & QStyle::State_Selected) {
        backgroundColor = palette.highlight().color();
        borderColor = palette.highlight().color().lighter(150);
        textColor = palette.highlightedText().color();
    } else if (option.state & QStyle::State_MouseOver) {
        backgroundColor = backgroundColor.lighter(120);
    }

    // bg
    painter->setBrush(backgroundColor);
    painter->setPen(QPen(borderColor, 1));
    painter->drawRoundedRect(cardRect, 10, 10);

    // icon
    int _iconsize = UISettings::values.game_icon_size.GetValue();
    QSize iconSize(_iconsize, _iconsize);
    QPixmap iconPixmap = index.data(Qt::DecorationRole).value<QPixmap>();

    QRect iconRect;
    if (!iconPixmap.isNull()) {
        QSize scaledSize = iconPixmap.size();
        scaledSize.scale(iconSize, Qt::KeepAspectRatio);

        int x = cardRect.left() + (cardRect.width() - scaledSize.width()) / 2;
        int y = cardRect.top() + margins;

        iconRect = QRect(x, y, scaledSize.width(), scaledSize.height());

        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

        // Put this in a separate thing on the painter stack to prevent clipping the text.
        painter->save();

        // round image edges
        QPainterPath path;
        path.addRoundedRect(iconRect, 10, 10);
        painter->setClipPath(path);

        painter->drawPixmap(iconRect, iconPixmap);

        painter->restore();
    } else {
        // if there is no icon just draw a blank rect
        iconRect = QRect(cardRect.left() + margins, cardRect.top() + margins, _iconsize, _iconsize);
    }

    if (UISettings::values.show_game_name.GetValue()) {
        // padding + text
        QRect textRect = cardRect;
        textRect.setTop(iconRect.bottom() + margins);
        textRect.adjust(margins, 0, -margins, -margins);

        // We are already crammed on space, ignore the row 2
        QString title = index.data(Qt::DisplayRole).toString();
        title = title.split(QLatin1Char('\n')).first();

        // now draw text
        painter->setPen(textColor);
        QFont font = option.font;
        font.setBold(true);

        // TODO(crueter): fix this abysmal scaling
        font.setPixelSize(1.5 + std::max(10.0, std::sqrt(_iconsize)));

        // TODO(crueter): elide mode
        painter->setFont(font);

        painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, title);
    }

    painter->restore();
}

QSize GameCard::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    return m_size;
}

void GameCard::setSize(const QSize& newSize, const int padding, const int columns) {
    m_size = newSize;
    m_padding = padding;
    m_columns = columns;
}
