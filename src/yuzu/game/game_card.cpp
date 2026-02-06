// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QPainter>
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

    // padding
    QRect cardRect = option.rect.adjusted(4, 4, -4, -4);

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
        backgroundColor = backgroundColor.lighter(110);
    }

    // bg
    painter->setBrush(backgroundColor);
    painter->setPen(QPen(borderColor, 1));
    painter->drawRoundedRect(cardRect, 10, 10);

    static constexpr const int padding = 10;

    // icon
    int _iconsize = UISettings::values.game_icon_size.GetValue();
    QSize iconSize(_iconsize, _iconsize);
    QPixmap iconPixmap = index.data(Qt::DecorationRole).value<QPixmap>();

    QRect iconRect;
    if (!iconPixmap.isNull()) {
        QSize scaledSize = iconPixmap.size();
        scaledSize.scale(iconSize, Qt::KeepAspectRatio);

        int x = cardRect.left() + (cardRect.width() - scaledSize.width()) / 2;
        int y = cardRect.top() + padding;

        iconRect = QRect(x, y, scaledSize.width(), scaledSize.height());

        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawPixmap(iconRect, iconPixmap);
    } else {
        // if there is no icon just draw a blank rect
        iconRect = QRect(cardRect.left() + padding,
                         cardRect.top() + padding,
                         _iconsize, _iconsize);
    }

    // if "none" is selected, pretend there's a
    _iconsize = _iconsize ? _iconsize : 96;

    // padding + text
    QRect textRect = cardRect;
    textRect.setTop(iconRect.bottom() + 8);
    textRect.adjust(padding, 0, -padding, -padding);

    // We are already crammed on space, ignore the row 2
    QString title = index.data(Qt::DisplayRole).toString();
    title = title.split(QLatin1Char('\n')).first();

    // now draw text
    painter->setPen(textColor);
    QFont font = option.font;
    font.setBold(true);

    // TODO(crueter): fix this abysmal scaling
    // If "none" is selected, then default to 8.5 point font.
    font.setPointSize(1 + std::max(7.0, _iconsize ? std::sqrt(_iconsize * 0.6) : 7.5));

    // TODO(crueter): elide mode
    painter->setFont(font);

    painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, title);

    painter->restore();
}

QSize GameCard::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    return m_size;
}

void GameCard::setSize(const QSize& newSize) {
    m_size = newSize;
}
