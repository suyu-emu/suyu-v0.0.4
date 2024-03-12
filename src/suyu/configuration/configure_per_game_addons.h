// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include <QList>
#include <qtreewidget.h>

#include "core/file_sys/patch_manager.h"
#include "core/file_sys/vfs/vfs_types.h"

namespace Core {
class System;
}

class QGraphicsScene;
class QStandardItem;
class QStandardItemModel;
class QTreeView;
class QVBoxLayout;

namespace Ui {
class ConfigurePerGameAddons;
}

class ConfigurePerGameAddons : public QWidget {
    Q_OBJECT

public:
    explicit ConfigurePerGameAddons(Core::System& system_, QWidget* parent = nullptr);
    ~ConfigurePerGameAddons() override;

    /// Save all button configurations to settings file
    void ApplyConfiguration();

    void LoadFromFile(FileSys::VirtualFile file_);

    void SetTitleId(u64 id);

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void LoadConfiguration();
    void ReloadList();

    std::unique_ptr<Ui::ConfigurePerGameAddons> ui;
    FileSys::VirtualFile file;
    u64 title_id;

    QVBoxLayout* layout;
    QTreeView* tree_view;
    QStandardItemModel* item_model;

    std::vector<QList<QStandardItem*>> list_items;

    std::optional<FileSys::Patch> selected_patch;
    std::vector<FileSys::Patch> patches;

    Core::System& system;

private slots:
    void OnPatchSelected(const QModelIndex& selectedIndex);
    void OnPatchCreateClick(bool checked = false);
    void OnPatchEditClick(bool checked = false);
    void OnPatchRemoveClick(bool checked = false);
    void OnPatchOpenFolder(bool checked = false);
};
