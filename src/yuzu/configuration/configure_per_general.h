// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <QKeyEvent>
#include <QList>
#include <QWidget>
#include <boost/optional.hpp>
#include "common/param_package.h"
#include "core/file_sys/vfs.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "ui_configure_per_general.h"
#include "yuzu/configuration/config.h"

class QTreeView;
class QGraphicsScene;
class QStandardItem;
class QStandardItemModel;

namespace Ui {
class ConfigurePerGameGeneral;
}

class ConfigurePerGameGeneral : public QDialog {
    Q_OBJECT

public:
    explicit ConfigurePerGameGeneral(u64 title_id, QWidget* parent = nullptr);

    /// Save all button configurations to settings file
    void applyConfiguration();

    void loadFromFile(FileSys::VirtualFile file);

private:
    std::unique_ptr<Ui::ConfigurePerGameGeneral> ui;
    FileSys::VirtualFile file;
    u64 title_id;

    QVBoxLayout* layout;
    QTreeView* tree_view;
    QStandardItemModel* item_model;
    QGraphicsScene* scene;

    std::vector<QList<QStandardItem*>> list_items;

    void loadConfiguration();
};
