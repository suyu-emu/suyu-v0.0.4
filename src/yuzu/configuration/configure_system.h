// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QGraphicsScene>
#include <QList>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

namespace Ui {
class ConfigureSystem;
}

class ConfigureSystem : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureSystem(QWidget* parent = nullptr);
    ~ConfigureSystem();

    void applyConfiguration();
    void setConfiguration();

    void UpdateCurrentUser();

public slots:
    void updateBirthdayComboBox(int birthmonth_index);
    void refreshConsoleID();

    void SelectUser(const QModelIndex& index);
    void AddUser();
    void RenameUser();
    void DeleteUser();

private:
    void ReadSystemSettings();

    QVBoxLayout* layout;
    QTreeView* tree_view;
    QStandardItemModel* item_model;
    QGraphicsScene* scene;

    std::vector<QList<QStandardItem*>> list_items;

    std::unique_ptr<Ui::ConfigureSystem> ui;
    bool enabled;

    std::u16string username;
    int birthmonth, birthday;
    int language_index;
    int sound_index;
};
