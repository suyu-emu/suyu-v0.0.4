// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStyledItemDelegate>

/**
 * A stylized "card"-like delegate for the game grid view.
 * Adapted from QML
 */
class GameCard : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit GameCard(QObject* parent = nullptr);

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
    void setSize(const QSize& newSize);

private:
    QSize m_size;
};
