// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QListView>
#include <QString>

#include "common/common_types.h"

class GameCard;
class GameListModel;

class GameGrid : public QListView {
    Q_OBJECT

public:
    explicit GameGrid(QWidget* parent = nullptr);

    void SetModel(GameListModel* model);
    void ApplyFilter(const QString& edit_filter_text, GameListModel* model);
    void UpdateIconSize();

private:
    GameCard* m_gameCard = nullptr;
};
