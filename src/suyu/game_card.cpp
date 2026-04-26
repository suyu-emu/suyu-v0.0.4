// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suyu/game_card.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOption>

GameCard::GameCard(QWidget* parent)
    : QWidget(parent), main_layout(nullptr), icon_label(nullptr), title_label(nullptr),
      developer_label(nullptr), version_label(nullptr), type_label(nullptr), size_label(nullptr),
      play_time_label(nullptr), compatibility_indicator(nullptr), program_id_numeric(0),
      file_size(0), is_selected(false), is_hovered(false), hover_animation(nullptr),
      shadow_effect(nullptr) {

    setFixedSize(CARD_WIDTH, CARD_HEIGHT);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, true);

    SetupUI();
    UpdateCardStyle();
}

GameCard::~GameCard() = default;

void GameCard::SetupUI() {
    main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(8);

    // Icon
    icon_label = new QLabel();
    icon_label->setFixedSize(ICON_SIZE, ICON_SIZE);
    icon_label->setAlignment(Qt::AlignCenter);
    icon_label->setScaledContents(true);
    icon_label->setPixmap(GetDefaultIcon());
    main_layout->addWidget(icon_label, 0, Qt::AlignHCenter);

    // Title
    title_label = new QLabel();
    title_label->setObjectName(QStringLiteral("title"));
    title_label->setAlignment(Qt::AlignCenter);
    title_label->setWordWrap(true);
    title_label->setMaximumHeight(40);
    QFont title_font = title_label->font();
    title_font.setPointSize(10);
    title_font.setBold(true);
    title_label->setFont(title_font);
    main_layout->addWidget(title_label);

    // Developer
    developer_label = new QLabel();
    developer_label->setObjectName(QStringLiteral("subtitle"));
    developer_label->setAlignment(Qt::AlignCenter);
    QFont dev_font = developer_label->font();
    dev_font.setPointSize(8);
    developer_label->setFont(dev_font);
    main_layout->addWidget(developer_label);

    // Version and Type
    QHBoxLayout* info_layout = new QHBoxLayout();
    info_layout->setSpacing(4);

    version_label = new QLabel();
    version_label->setObjectName(QStringLiteral("subtitle"));
    QFont version_font = version_label->font();
    version_font.setPointSize(7);
    version_label->setFont(version_font);
    info_layout->addWidget(version_label);

    type_label = new QLabel();
    type_label->setObjectName(QStringLiteral("subtitle"));
    QFont type_font = type_label->font();
    type_font.setPointSize(7);
    type_label->setFont(type_font);
    info_layout->addWidget(type_label);

    main_layout->addLayout(info_layout);

    // Size and Play Time
    QHBoxLayout* stats_layout = new QHBoxLayout();
    stats_layout->setSpacing(4);

    size_label = new QLabel();
    size_label->setObjectName(QStringLiteral("subtitle"));
    QFont size_font = size_label->font();
    size_font.setPointSize(7);
    size_label->setFont(size_font);
    stats_layout->addWidget(size_label);

    play_time_label = new QLabel();
    play_time_label->setObjectName(QStringLiteral("subtitle"));
    QFont time_font = play_time_label->font();
    time_font.setPointSize(7);
    play_time_label->setFont(time_font);
    stats_layout->addWidget(play_time_label);

    main_layout->addLayout(stats_layout);

    // Compatibility indicator
    compatibility_indicator = new QLabel();
    compatibility_indicator->setFixedSize(12, 12);
    compatibility_indicator->setStyleSheet(QStringLiteral("border-radius: 6px;"));
    main_layout->addWidget(compatibility_indicator, 0, Qt::AlignHCenter);

    main_layout->addStretch();

    // Shadow effect
    shadow_effect = new QGraphicsDropShadowEffect();
    shadow_effect->setBlurRadius(10);
    shadow_effect->setColor(QColor(0, 0, 0, 80));
    shadow_effect->setOffset(0, 2);
    setGraphicsEffect(shadow_effect);

    // Hover animation
    hover_animation = new QPropertyAnimation(shadow_effect, "blurRadius");
    hover_animation->setDuration(200);
    connect(hover_animation, &QPropertyAnimation::finished, this, &GameCard::OnHoverAnimationFinished);
}

void GameCard::SetGameInfo(const QString& title, const QString& game_path, const QString& pid,
                          const QString& dev, u64 pid_numeric, const QString& ver,
                          const QString& game_type, u64 size, const QString& compat,
                          const QPixmap& icon) {
    this->game_title = title;
    this->file_path = game_path;
    this->program_id = pid;
    this->developer = dev;
    this->program_id_numeric = pid_numeric;
    this->version = ver;
    this->type = game_type;
    this->file_size = size;
    this->compatibility = compat;
    this->game_icon = icon;

    SetTitle(title);
    SetIcon(icon);
    SetDeveloper(dev);
    SetVersion(ver);
    SetType(game_type);
    SetSize(size);
    SetCompatibility(compat);
}

void GameCard::SetTitle(const QString& title) {
    game_title = title;
    if (title_label) {
        // Truncate title if too long
        QFontMetrics metrics(title_label->font());
        QString elided_text = metrics.elidedText(title, Qt::ElideRight, title_label->width());
        title_label->setText(elided_text);
        title_label->setToolTip(title);
    }
}

void GameCard::SetIcon(const QPixmap& icon) {
    game_icon = icon;
    if (icon_label) {
        if (icon.isNull()) {
            icon_label->setPixmap(GetDefaultIcon());
        } else {
            QPixmap scaled_icon = icon.scaled(ICON_SIZE, ICON_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            icon_label->setPixmap(scaled_icon);
        }
    }
}

void GameCard::SetCompatibility(const QString& compat) {
    this->compatibility = compat;
    if (compatibility_indicator) {
        QColor color = GetCompatibilityColor(compat);
        compatibility_indicator->setStyleSheet(
            QStringLiteral("background-color: %1; border-radius: 6px;").arg(color.name()));
        compatibility_indicator->setToolTip(QStringLiteral("Compatibility: %1").arg(compat));
    }
}

void GameCard::SetPlayTime(const QString& time_str) {
    this->play_time = time_str;
    if (play_time_label) {
        play_time_label->setText(time_str);
    }
}

void GameCard::SetSize(u64 size) {
    file_size = size;
    if (size_label) {
        size_label->setText(FormatFileSize(size));
    }
}

void GameCard::SetType(const QString& type_str) {
    this->type = type_str;
    if (type_label) {
        type_label->setText(type_str);
    }
}

void GameCard::SetVersion(const QString& ver_str) {
    this->version = ver_str;
    if (version_label) {
        version_label->setText(QStringLiteral("v%1").arg(ver_str));
    }
}

void GameCard::SetDeveloper(const QString& dev_str) {
    this->developer = dev_str;
    if (developer_label) {
        QFontMetrics metrics(developer_label->font());
        QString elided_text = metrics.elidedText(dev_str, Qt::ElideRight, developer_label->width());
        developer_label->setText(elided_text);
        developer_label->setToolTip(dev_str);
    }
}

QString GameCard::GetTitle() const {
    return game_title;
}

QString GameCard::GetFilePath() const {
    return file_path;
}

QString GameCard::GetProgramId() const {
    return program_id;
}

u64 GameCard::GetProgramIdNumeric() const {
    return program_id_numeric;
}

void GameCard::SetSelected(bool selected) {
    if (is_selected != selected) {
        is_selected = selected;
        UpdateCardStyle();
        update();
    }
}

bool GameCard::IsSelected() const {
    return is_selected;
}

void GameCard::SetHovered(bool hovered) {
    if (is_hovered != hovered) {
        is_hovered = hovered;
        StartHoverAnimation(hovered);
        UpdateCardStyle();
        update();
    }
}

bool GameCard::IsHovered() const {
    return is_hovered;
}

void GameCard::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        SetSelected(true);
        emit GameSelected(file_path);
    }
    QWidget::mousePressEvent(event);
}

void GameCard::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit GameDoubleClicked(file_path);
    }
    QWidget::mouseDoubleClickEvent(event);
}

void GameCard::contextMenuEvent(QContextMenuEvent* event) {
    emit GameRightClicked(file_path, event->globalPos());
}

void GameCard::enterEvent(QEvent* event) {
    SetHovered(true);
    QWidget::enterEvent(event);
}

void GameCard::leaveEvent(QEvent* event) {
    SetHovered(false);
    QWidget::leaveEvent(event);
}

void GameCard::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw card background
    QPainterPath path;
    path.addRoundedRect(rect(), 14, 14);

    QColor bg_color = QColor(32, 24, 48);
    QColor border_color = Qt::transparent;
    if (is_selected) {
        bg_color = QColor(62, 40, 96);
        border_color = QColor(175, 110, 255, 220);
    } else if (is_hovered) {
        bg_color = QColor(46, 34, 78);
        border_color = QColor(175, 110, 255, 160);
    }

    QLinearGradient backgroundGradient(rect().topLeft(), rect().bottomRight());
    backgroundGradient.setColorAt(0.0, bg_color.lighter(110));
    backgroundGradient.setColorAt(1.0, bg_color.darker(105));
    painter.fillPath(path, backgroundGradient);

    if (border_color.alpha() > 0) {
        painter.setPen(QPen(border_color, 2));
    } else {
        painter.setPen(Qt::NoPen);
    }
    painter.drawPath(path);

    QWidget::paintEvent(event);
}

void GameCard::focusInEvent(QFocusEvent* event) {
    SetSelected(true);
    QWidget::focusInEvent(event);
}

void GameCard::focusOutEvent(QFocusEvent* event) {
    if (!is_hovered) {
        SetSelected(false);
    }
    QWidget::focusOutEvent(event);
}

void GameCard::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit GameDoubleClicked(file_path);
    } else if (event->key() == Qt::Key_Menu) {
        QPoint global_pos = mapToGlobal(rect().center());
        emit GameRightClicked(file_path, global_pos);
    }
    QWidget::keyPressEvent(event);
}

void GameCard::OnHoverAnimationFinished() {
    // Animation finished, nothing special to do
}

void GameCard::UpdateCardStyle() {
    setProperty("selected", is_selected);
    setProperty("hovered", is_hovered);
    style()->unpolish(this);
    style()->polish(this);
}

void GameCard::StartHoverAnimation(bool hover_in) {
    if (!hover_animation || !shadow_effect) {
        return;
    }

    hover_animation->stop();

    if (hover_in) {
        hover_animation->setStartValue(shadow_effect->blurRadius());
        hover_animation->setEndValue(15.0);
    } else {
        hover_animation->setStartValue(shadow_effect->blurRadius());
        hover_animation->setEndValue(10.0);
    }

    hover_animation->start();
}

QString GameCard::FormatFileSize(u64 size) const {
    static const QString units[] = {
        QStringLiteral("B"), QStringLiteral("KB"), QStringLiteral("MB"),
        QStringLiteral("GB"), QStringLiteral("TB")
    };
    int unit_index = 0;
    double size_double = static_cast<double>(size);

    while (size_double >= 1024.0 && unit_index < 4) {
        size_double /= 1024.0;
        unit_index++;
    }

    return QStringLiteral("%1 %2").arg(QString::number(size_double, 'f', 1)).arg(units[unit_index]);
}

QColor GameCard::GetCompatibilityColor(const QString& compat_str) const {
    if (compat_str == QStringLiteral("Perfect")) {
        return QColor(0, 255, 0); // Green
    } else if (compat_str == QStringLiteral("Great")) {
        return QColor(50, 205, 50); // Lime green
    } else if (compat_str == QStringLiteral("Okay")) {
        return QColor(255, 255, 0); // Yellow
    } else if (compat_str == QStringLiteral("Bad")) {
        return QColor(255, 165, 0); // Orange
    } else if (compat_str == QStringLiteral("Intro/Menu")) {
        return QColor(255, 0, 0); // Red
    } else {
        return QColor(128, 128, 128); // Gray for unknown
    }
}

QPixmap GameCard::GetDefaultIcon() {
    static QPixmap icon;
    if (icon.isNull()) {
        // Create a default icon if none exists
        icon = QPixmap(ICON_SIZE, ICON_SIZE);
        icon.fill(Qt::transparent);

        QPainter painter(&icon);
        painter.setRenderHint(QPainter::Antialiasing);

        // Draw a simple game controller icon
        painter.setPen(QPen(QColor(96, 96, 96), 2));
        painter.setBrush(QBrush(QColor(64, 64, 64)));

        QRect icon_rect(16, 32, 96, 64);
        painter.drawRoundedRect(icon_rect, 8, 8);

        // Draw some buttons
        painter.setBrush(QBrush(QColor(96, 96, 96)));
        painter.drawEllipse(32, 48, 12, 12);
        painter.drawEllipse(48, 48, 12, 12);
        painter.drawEllipse(68, 48, 12, 12);
        painter.drawEllipse(84, 48, 12, 12);
    }
    return icon;
}

// GameCardLayout implementation
GameCardLayout::GameCardLayout(QWidget* parent) : QLayout(parent), m_spacing(-1) {
}

GameCardLayout::~GameCardLayout() {
    QLayoutItem* item;
    while ((item = takeAt(0))) {
        delete item;
    }
}

void GameCardLayout::addItem(QLayoutItem* item) {
    item_list.append(item);
}

QSize GameCardLayout::sizeHint() const {
    return minimumSize();
}

QSize GameCardLayout::minimumSize() const {
    QSize size;
    for (const QLayoutItem* item : item_list) {
        size = size.expandedTo(item->minimumSize());
    }

    const QMargins margins = contentsMargins();
    size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    return size;
}

int GameCardLayout::count() const {
    return item_list.size();
}

QLayoutItem* GameCardLayout::itemAt(int index) const {
    return item_list.value(index);
}

QLayoutItem* GameCardLayout::takeAt(int index) {
    if (index >= 0 && index < item_list.size()) {
        return item_list.takeAt(index);
    }
    return nullptr;
}

void GameCardLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

void GameCardLayout::setSpacing(int spacing) {
    m_spacing = spacing;
}

int GameCardLayout::spacing() const {
    if (m_spacing >= 0) {
        return m_spacing;
    } else {
        return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
    }
}

int GameCardLayout::doLayout(const QRect& rect, bool testOnly) const {
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
    int x = effectiveRect.x();
    int y = effectiveRect.y();
    int lineHeight = 0;

    for (QLayoutItem* item : item_list) {
        const QWidget* wid = item->widget();
        int spaceX = spacing();
        int spaceY = spacing();

        if (wid) {
            spaceX = wid->style()->layoutSpacing(QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Horizontal);
            spaceY = wid->style()->layoutSpacing(QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Vertical);
        }

        int nextX = x + item->sizeHint().width() + spaceX;
        if (nextX - spaceX > effectiveRect.right() && lineHeight > 0) {
            x = effectiveRect.x();
            y = y + lineHeight + spaceY;
            nextX = x + item->sizeHint().width() + spaceX;
            lineHeight = 0;
        }

        if (!testOnly) {
            item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));
        }

        x = nextX;
        lineHeight = qMax(lineHeight, item->sizeHint().height());
    }
    return y + lineHeight - rect.y() + bottom;
}

int GameCardLayout::smartSpacing(QStyle::PixelMetric pm) const {
    QObject* parent = this->parent();
    if (!parent) {
        return -1;
    } else if (parent->isWidgetType()) {
        QWidget* pw = static_cast<QWidget*>(parent);
        return pw->style()->pixelMetric(pm, nullptr, pw);
    } else {
        return static_cast<QLayout*>(parent)->spacing();
    }
}

#include "game_card.moc"