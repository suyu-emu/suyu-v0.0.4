// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>

#include <QDialog>
#include <QList>
#include <QWidget>
#include "core/file_sys/vfs/vfs_types.h"

namespace Common {
struct UUID;
}

namespace Core {
class System;
}

class QGraphicsScene;
class QDialogButtonBox;
class QLabel;
class QStandardItem;
class QStandardItemModel;
class QTreeView;
class QVBoxLayout;
class QListWidget;

namespace Service::Account {
class ProfileManager;
}

namespace Ui {
class ConfigureProfileManager;
}

class ConfigureProfileManagerAvatarDialog : public QDialog {
public:
    explicit ConfigureProfileManagerAvatarDialog(QWidget* parent);
    ~ConfigureProfileManagerAvatarDialog();

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

class ConfigureProfileManagerDeleteDialog : public QDialog {
public:
    explicit ConfigureProfileManagerDeleteDialog(QWidget* parent);
    ~ConfigureProfileManagerDeleteDialog();

    void SetInfo(const QString& username, const Common::UUID& uuid,
                 std::function<void()> accept_callback);

private:
    QDialogButtonBox* dialog_button_box;
    QGraphicsScene* icon_scene;
    QLabel* label_info;
};

class ConfigureProfileManager : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureProfileManager(Core::System& system_, QWidget* parent = nullptr);
    ~ConfigureProfileManager() override;

    void ApplyConfiguration();

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void SetConfiguration();

    void PopulateUserList();
    void UpdateCurrentUser();

    void SelectUser(const QModelIndex& index);
    void AddUser();
    void RenameUser();
    void ConfirmDeleteUser();
    void DeleteUser(const Common::UUID& uuid);
    void SetUserImage(const QImage& image);
    void SelectImageFile();
    void SelectFirmwareAvatar();
    bool LoadAvatarData();
    std::vector<uint8_t> DecompressYaz0(const FileSys::VirtualFile& file);

    QVBoxLayout* layout;
    QTreeView* tree_view;
    QStandardItemModel* item_model;
    QGraphicsScene* scene;

    ConfigureProfileManagerAvatarDialog* avatar_dialog;
    ConfigureProfileManagerDeleteDialog* confirm_dialog;

    std::vector<QList<QStandardItem*>> list_items;

    std::unique_ptr<Ui::ConfigureProfileManager> ui;
    bool enabled = false;

    Service::Account::ProfileManager& profile_manager;
    const Core::System& system;
};
