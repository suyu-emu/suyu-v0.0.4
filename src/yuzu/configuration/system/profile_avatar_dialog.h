// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>

class QListWidget;

class ProfileAvatarDialog : public QDialog {
public:
    explicit ProfileAvatarDialog(QWidget* parent);
    ~ProfileAvatarDialog();

    void LoadImages(const QVector<QPixmap>& avatar_images);
    bool AreImagesLoaded() const;
    QPixmap GetSelectedAvatar();

private:
    void SetBackgroundColor(const QColor& color);
    QPixmap CreateAvatar(const QPixmap& avatar);
    void RefreshAvatars();

    QVector<QPixmap> avatar_image_store;
    QListWidget* avatar_list;
    QColor avatar_bg_color;
    QPushButton* bg_color_button;
};
