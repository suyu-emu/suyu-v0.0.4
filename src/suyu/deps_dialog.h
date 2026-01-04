// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QStyledItemDelegate>
#include <QTableView>
#include <memory>

namespace Ui { class DepsDialog; }

class DepsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DepsDialog(QWidget *parent);
    ~DepsDialog() override;

private:
    std::unique_ptr<Ui::DepsDialog> ui;
};

class LinkItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit LinkItemDelegate(QObject *parent = 0);

protected:
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool editorEvent(QEvent *event,
                     QAbstractItemModel *model,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;
};
