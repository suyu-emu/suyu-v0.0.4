// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QPushButton>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QStyle>

#include "common/common_types.h"
#include "suyu/compatibility_list.h"

class GameCard : public QWidget {
    Q_OBJECT

public:
    explicit GameCard(QWidget* parent = nullptr);
    ~GameCard() override;

    void SetGameInfo(const QString& title, const QString& game_path, const QString& pid,
                     const QString& dev, u64 pid_numeric, const QString& ver,
                     const QString& game_type, u64 size, const QString& compat,
                     const QPixmap& icon);

    void SetTitle(const QString& title);
    void SetIcon(const QPixmap& icon);
    void SetCompatibility(const QString& compat);
    void SetPlayTime(const QString& time_str);
    void SetSize(u64 size);
    void SetType(const QString& type_str);
    void SetVersion(const QString& ver_str);
    void SetDeveloper(const QString& dev_str);

    QString GetTitle() const;
    QString GetFilePath() const;
    QString GetProgramId() const;
    u64 GetProgramIdNumeric() const;

    void SetSelected(bool selected);
    bool IsSelected() const;

    void SetHovered(bool hovered);
    bool IsHovered() const;

    // Size management
    static constexpr int CARD_WIDTH = 220;
    static constexpr int CARD_HEIGHT = 310;
    static constexpr int ICON_SIZE = 180;

signals:
    void GameSelected(const QString& file_path);
    void GameDoubleClicked(const QString& file_path);
    void GameRightClicked(const QString& file_path, const QPoint& global_pos);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void OnHoverAnimationFinished();

private:
    void SetupUI();
    void UpdateCardStyle();
    void StartHoverAnimation(bool hover_in);
    QString FormatFileSize(u64 size) const;
    QColor GetCompatibilityColor(const QString& compat_str) const;

    // UI Components
    QVBoxLayout* main_layout;
    QLabel* icon_label;
    QLabel* title_label;
    QLabel* developer_label;
    QLabel* version_label;
    QLabel* type_label;
    QLabel* size_label;
    QLabel* play_time_label;
    QLabel* compatibility_indicator;

    // Game Information
    QString game_title;
    QString file_path;
    QString program_id;
    QString developer;
    QString version;
    QString type;
    QString compatibility;
    QString play_time;
    u64 program_id_numeric;
    u64 file_size;
    QPixmap game_icon;

    // State
    bool is_selected;
    bool is_hovered;

    // Animation
    QPropertyAnimation* hover_animation;
    QGraphicsDropShadowEffect* shadow_effect;

    // Default icon (lazy-initialized, see GetDefaultIcon())
    static QPixmap GetDefaultIcon();
};

// Custom flow layout for game cards
class GameCardLayout : public QLayout {
    Q_OBJECT

public:
    explicit GameCardLayout(QWidget* parent = nullptr);
    ~GameCardLayout() override;

    void addItem(QLayoutItem* item) override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    int count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    void setGeometry(const QRect& rect) override;

    void setSpacing(int spacing);
    int spacing() const;

private:
    int doLayout(const QRect& rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem*> item_list;
    int m_spacing;
};