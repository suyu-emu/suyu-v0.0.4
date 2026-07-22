// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

class GameList;
class QHBoxLayout;
class QTreeView;
class QLabel;
class QLineEdit;
class QToolButton;

class GameListSearchField : public QWidget {
    Q_OBJECT

public:
    explicit GameListSearchField(GameList* parent = nullptr);

    QString filterText() const;
    void setFilterResult(int visible_, int total_);

    void clear();
    void setFocus();

private:
    void changeEvent(QEvent*) override;
    void RetranslateUI();

    class KeyReleaseEater : public QObject {
    public:
        explicit KeyReleaseEater(GameList* gamelist_, QObject* parent = nullptr);

    private:
        GameList* gamelist = nullptr;
        QString edit_filter_text_old;

    protected:
        // EventFilter in order to process systemkeys while editing the searchfield
        bool eventFilter(QObject* obj, QEvent* event) override;
    };
    int visible;
    int total;

    QHBoxLayout* layout_filter = nullptr;
    QTreeView* tree_view = nullptr;
    QLabel* label_filter = nullptr;
    QLineEdit* edit_filter = nullptr;
    QLabel* label_filter_result = nullptr;
    QToolButton* button_filter_close = nullptr;
};
