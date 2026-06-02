// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHeaderView>
#include <QString>
#include <QTreeView>

class GameListModel;

class GameTree : public QTreeView {
    Q_OBJECT

public:
    explicit GameTree(QWidget* parent = nullptr);

    void SetModel(GameListModel* model);

    QString GetLastFilterResultItem() const;
    int FilterClosedResultCount(GameListModel* model);
    void ApplyFilter(const QString& edit_filter_text, GameListModel* model);

    void SaveInterfaceLayout();
    void LoadInterfaceLayout();

    void UpdateColumnVisibility(GameListModel* model);

signals:
    void ItemExpandedChanged(const QModelIndex& item);
    void FilterResultReady(int visible, int total);

private slots:
    void OnItemExpanded(const QModelIndex& item);
};
