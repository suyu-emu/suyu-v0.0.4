// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QList>
#include <QWidget>

class QGraphicsScene;
class QStandardItem;
class QStandardItemModel;
class QTreeView;
class QVBoxLayout;

namespace Service::Account {
class ProfileManager;
}

namespace Ui {
class ConfigureSystem;
}

class ConfigureSystem : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureSystem(QWidget* parent = nullptr);
    ~ConfigureSystem() override;

    void applyConfiguration();
    void setConfiguration();

private:
    void ReadSystemSettings();

    void UpdateBirthdayComboBox(int birthmonth_index);
    void RefreshConsoleID();

    void PopulateUserList();
    void UpdateCurrentUser();
    void SelectUser(const QModelIndex& index);
    void AddUser();
    void RenameUser();
    void DeleteUser();
    void SetUserImage();

    QVBoxLayout* layout;
    QTreeView* tree_view;
    QStandardItemModel* item_model;
    QGraphicsScene* scene;

    std::vector<QList<QStandardItem*>> list_items;

    std::unique_ptr<Ui::ConfigureSystem> ui;
    bool enabled = false;

    int birthmonth = 0;
    int birthday = 0;
    int language_index = 0;
    int sound_index = 0;

    std::unique_ptr<Service::Account::ProfileManager> profile_manager;
};
