// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
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

class ProfileAvatarDialog;
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

// TODO(crueter): Move this to a .ui def
class ConfigureProfileManagerDeleteDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConfigureProfileManagerDeleteDialog(QWidget* parent);
    ~ConfigureProfileManagerDeleteDialog();

    void SetInfo(const QString& username, const Common::UUID& uuid,
                 int index);

signals:
    void deleteUser(int index);

private:
    QDialogButtonBox* dialog_button_box;
    QGraphicsScene* icon_scene;
    QLabel* label_info;
    int m_index = 0;
};

class ConfigureProfileManager : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureProfileManager(Core::System& system_, QWidget* parent = nullptr);
    ~ConfigureProfileManager() override;

    void ApplyConfiguration();

private slots:
    void saveImage(QPixmap pixmap, Common::UUID uuid);
    void showContextMenu(const QPoint &pos);
    void DeleteUser(const int index);

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void SetConfiguration();

    void PopulateUserList();
    void UpdateCurrentUser();

    void SelectUser(const QModelIndex& index);
    void AddUser();
    void EditUser();
    void ConfirmDeleteUser();
    bool LoadAvatarData();
    std::vector<uint8_t> DecompressYaz0(const FileSys::VirtualFile& file);

    QVBoxLayout* layout;
    QTreeView* tree_view;
    QStandardItemModel* item_model;
    QGraphicsScene* scene;
    ConfigureProfileManagerDeleteDialog* confirm_dialog;

    std::vector<QList<QStandardItem*>> list_items;

    std::unique_ptr<Ui::ConfigureProfileManager> ui;
    bool enabled = false;

    Service::Account::ProfileManager& profile_manager;
    const Core::System& system;
};
