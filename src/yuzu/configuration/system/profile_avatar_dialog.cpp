// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "profile_avatar_dialog.h"

#include <QBoxLayout>
#include <QColorDialog>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>

// TODO: Move this to a .ui def
ProfileAvatarDialog::ProfileAvatarDialog(QWidget* parent)
    : QDialog{parent}, avatar_list{new QListWidget(this)}, bg_color_button{new QPushButton(this)} {
    auto* main_layout = new QVBoxLayout(this);
    auto* button_layout = new QHBoxLayout();
    auto* select_button = new QPushButton(tr("Select"), this);
    auto* cancel_button = new QPushButton(tr("Cancel"), this);
    auto* bg_color_label = new QLabel(tr("Background Color"), this);

    SetBackgroundColor(Qt::white);

    avatar_list->setViewMode(QListView::IconMode);
    avatar_list->setIconSize(QSize(64, 64));
    avatar_list->setSpacing(4);
    avatar_list->setResizeMode(QListView::Adjust);
    avatar_list->setSelectionMode(QAbstractItemView::SingleSelection);
    avatar_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    avatar_list->setDragDropMode(QAbstractItemView::NoDragDrop);
    avatar_list->setDragEnabled(false);
    avatar_list->setDropIndicatorShown(false);
    avatar_list->setAcceptDrops(false);

    button_layout->addWidget(bg_color_button);
    button_layout->addWidget(bg_color_label);
    button_layout->addStretch();
    button_layout->addWidget(select_button);
    button_layout->addWidget(cancel_button);

    this->setWindowTitle(tr("Select Firmware Avatar"));
    main_layout->addWidget(avatar_list);
    main_layout->addLayout(button_layout);

    connect(bg_color_button, &QPushButton::clicked, this, [this]() {
        const auto new_color = QColorDialog::getColor(avatar_bg_color);
        if (new_color.isValid()) {
            SetBackgroundColor(new_color);
            RefreshAvatars();
        }
    });
    connect(select_button, &QPushButton::clicked, this, [this]() { accept(); });
    connect(cancel_button, &QPushButton::clicked, this, [this]() { reject(); });
}

ProfileAvatarDialog::~ProfileAvatarDialog() = default;

void ProfileAvatarDialog::SetBackgroundColor(const QColor& color) {
    avatar_bg_color = color;

    bg_color_button->setStyleSheet(
        QStringLiteral("background-color: %1; min-width: 60px;").arg(avatar_bg_color.name()));
}

QPixmap ProfileAvatarDialog::CreateAvatar(const QPixmap& avatar) {
    QPixmap output(avatar.size());
    output.fill(avatar_bg_color);

    // Scale the image and fill it black to become our shadow
    QPixmap shadow_pixmap = avatar.transformed(QTransform::fromScale(1.04, 1.04));
    QPainter shadow_painter(&shadow_pixmap);
    shadow_painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    shadow_painter.fillRect(shadow_pixmap.rect(), Qt::black);
    shadow_painter.end();

    QPainter painter(&output);
    painter.setOpacity(0.10);
    painter.drawPixmap(0, 0, shadow_pixmap);
    painter.setOpacity(1.0);
    painter.drawPixmap(0, 0, avatar);
    painter.end();

    return output;
}

void ProfileAvatarDialog::RefreshAvatars() {
    if (avatar_list->count() != avatar_image_store.size()) {
        return;
    }
    for (int i = 0; i < avatar_image_store.size(); ++i) {
        const auto icon =
            QIcon(CreateAvatar(avatar_image_store[i])
                      .scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        avatar_list->item(i)->setIcon(icon);
    }
}

void ProfileAvatarDialog::LoadImages(const QVector<QPixmap>& avatar_images) {
    avatar_image_store = avatar_images;
    avatar_list->clear();

    for (int i = 0; i < avatar_image_store.size(); ++i) {
        avatar_list->addItem(new QListWidgetItem);
    }
    RefreshAvatars();

    // Determine window size now that avatars are loaded into the grid
    // There is probably a better way to handle this that I'm unaware of
    const auto* style = avatar_list->style();

    const int icon_size = avatar_list->iconSize().width();
    const int icon_spacing = avatar_list->spacing() * 2;
    const int icon_margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin);
    const int icon_full_size = icon_size + icon_spacing + icon_margin;

    const int horizontal_margin = style->pixelMetric(QStyle::PM_LayoutLeftMargin) +
                                  style->pixelMetric(QStyle::PM_LayoutRightMargin) +
                                  style->pixelMetric(QStyle::PM_ScrollBarExtent);
    const int vertical_margin = style->pixelMetric(QStyle::PM_LayoutTopMargin) +
                                style->pixelMetric(QStyle::PM_LayoutBottomMargin);

    // Set default list size so that it is 6 icons wide and 4.5 tall
    const int columns = 6;
    const double rows = 4.5;
    const int total_width = icon_full_size * columns + horizontal_margin;
    const int total_height = icon_full_size * rows + vertical_margin;
    avatar_list->setMinimumSize(total_width, total_height);
}

bool ProfileAvatarDialog::AreImagesLoaded() const {
    return !avatar_image_store.isEmpty();
}

QPixmap ProfileAvatarDialog::GetSelectedAvatar() {
    return CreateAvatar(avatar_image_store[avatar_list->currentRow()]);
}
