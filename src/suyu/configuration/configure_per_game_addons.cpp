// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <utility>

#include <QHeaderView>
#include <QMenu>
#include <QStandardItemModel>
#include <QString>
#include <QTimer>
#include <QTreeView>
#include <qdesktopservices.h>
#include <qdialog.h>
#include <qdialogbuttonbox.h>
#include <qformlayout.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qtreewidget.h>

#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/file_sys/patch_manager.h"
#include "core/loader/loader.h"
#include "suyu/configuration/configure_input.h"
#include "suyu/configuration/configure_per_game_addons.h"
#include "suyu/uisettings.h"
#include "ui_configure_per_game_addons.h"

ConfigurePerGameAddons::ConfigurePerGameAddons(Core::System& system_, QWidget* parent)
    : QWidget(parent), ui{std::make_unique<Ui::ConfigurePerGameAddons>()}, system{system_} {
    ui->setupUi(this);

    layout = new QVBoxLayout;
    tree_view = new QTreeView;
    item_model = new QStandardItemModel(tree_view);
    tree_view->setModel(item_model);
    tree_view->setAlternatingRowColors(true);
    tree_view->setSelectionMode(QHeaderView::SingleSelection);
    tree_view->setSelectionBehavior(QHeaderView::SelectRows);
    tree_view->setVerticalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setHorizontalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setSortingEnabled(true);
    tree_view->setEditTriggers(QHeaderView::NoEditTriggers);
    tree_view->setUniformRowHeights(true);
    tree_view->setContextMenuPolicy(Qt::NoContextMenu);

    item_model->insertColumns(0, 2);
    item_model->setHeaderData(0, Qt::Horizontal, tr("Patch Name"));
    item_model->setHeaderData(1, Qt::Horizontal, tr("Version"));

    tree_view->header()->setStretchLastSection(false);
    tree_view->header()->setSectionResizeMode(0, QHeaderView::ResizeMode::Stretch);
    tree_view->header()->setMinimumSectionSize(150);

    // We must register all custom types with the Qt Automoc system so that we are able to use it
    // with signals/slots. In this case, QList falls under the umbrella of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tree_view);

    ui->scrollArea->setLayout(layout);

    ui->scrollArea->setEnabled(!system.IsPoweredOn());

    connect(item_model, &QStandardItemModel::itemChanged,
            [] { UISettings::values.is_game_list_reload_pending.exchange(true); });

    connect(tree_view, &QTreeView::clicked, this, &ConfigurePerGameAddons::OnPatchSelected);

    connect(ui->new_btn, &QPushButton::clicked, this, &ConfigurePerGameAddons::OnPatchCreateClick);
    connect(ui->edit_btn, &QPushButton::clicked, this, &ConfigurePerGameAddons::OnPatchEditClick);
    connect(ui->remove_btn, &QPushButton::clicked, this,
            &ConfigurePerGameAddons::OnPatchRemoveClick);

    connect(ui->folder_btn, &QPushButton::clicked, this,
            &ConfigurePerGameAddons::OnPatchOpenFolder);
}

ConfigurePerGameAddons::~ConfigurePerGameAddons() = default;

void ConfigurePerGameAddons::ApplyConfiguration() {
    std::vector<std::string> disabled_addons;

    for (const auto& item : list_items) {
        const auto disabled = item.front()->checkState() == Qt::Unchecked;
        if (disabled)
            disabled_addons.push_back(item.front()->text().toStdString());
    }

    auto current = Settings::values.disabled_addons[title_id];
    std::sort(disabled_addons.begin(), disabled_addons.end());
    std::sort(current.begin(), current.end());
    if (disabled_addons != current) {
        Common::FS::RemoveFile(Common::FS::GetSuyuPath(Common::FS::SuyuPath::CacheDir) /
                               "game_list" / fmt::format("{:016X}.pv.txt", title_id));
    }

    Settings::values.disabled_addons[title_id] = disabled_addons;
}

void ConfigurePerGameAddons::LoadFromFile(FileSys::VirtualFile file_) {
    file = std::move(file_);
    LoadConfiguration();
}

void ConfigurePerGameAddons::SetTitleId(u64 id) {
    this->title_id = id;
}

void ConfigurePerGameAddons::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigurePerGameAddons::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigurePerGameAddons::LoadConfiguration() {
    if (file == nullptr) {
        return;
    }

    const FileSys::PatchManager pm{title_id, system.GetFileSystemController(),
                                   system.GetContentProvider()};
    const auto loader = Loader::GetLoader(system, file);

    FileSys::VirtualFile update_raw;
    loader->ReadUpdateRaw(update_raw);
    patches = pm.GetPatches(update_raw);

    const auto& disabled = Settings::values.disabled_addons[title_id];

    for (const auto& patch : patches) {
        const auto name = QString::fromStdString(patch.name);

        auto* const first_item = new QStandardItem;
        first_item->setText(name);
        first_item->setCheckable(true);

        const auto patch_disabled =
            std::find(disabled.begin(), disabled.end(), name.toStdString()) != disabled.end();

        first_item->setCheckState(patch_disabled ? Qt::Unchecked : Qt::Checked);

        list_items.push_back(QList<QStandardItem*>{
            first_item, new QStandardItem{QString::fromStdString(patch.version)}});
        item_model->appendRow(list_items.back());
    }

    tree_view->resizeColumnToContents(1);
}

void ConfigurePerGameAddons::ReloadList() {
    // Clear all items and selection
    item_model->setRowCount(0);
    list_items.clear();
    selected_patch = std::nullopt;

    // Remove the cache to ensure we'll recreate it
    Common::FS::RemoveFile(Common::FS::GetSuyuPath(Common::FS::SuyuPath::CacheDir) / "game_list" /
                           fmt::format("{:016X}.pv.txt", title_id));

    // Reload stuff
    UISettings::values.is_game_list_reload_pending.exchange(true);
    UISettings::values.is_game_list_reload_pending.notify_all();
    LoadConfiguration();
    ApplyConfiguration();
}

void ConfigurePerGameAddons::OnPatchSelected(const QModelIndex& selectedIndex) {
    QModelIndexList indexes = tree_view->selectionModel()->selectedIndexes();
    if (indexes.size() == 0) {
        // Nothing selected
        ui->edit_btn->setEnabled(false);
        ui->remove_btn->setEnabled(false);
        return;
    }

    QStandardItemModel* model = (QStandardItemModel*)tree_view->model();
    QStandardItem* item = model->itemFromIndex(selectedIndex.siblingAtColumn(0));

    std::string patch_name = item->text().toStdString();
    selected_patch = std::nullopt;

    for (const auto& patch : patches) {
        if (patch.name != patch_name)
            continue;
        if (patch.version != "IPSwitch")
            continue;

        selected_patch = patch;
    }

    if (!selected_patch || !selected_patch->file_path) {
        // Either patch not found or selected isn't a patch
        ui->edit_btn->setEnabled(false);
        ui->remove_btn->setEnabled(false);
        return;
    }

    ui->edit_btn->setEnabled(true);
    ui->remove_btn->setEnabled(true);
}

void ConfigurePerGameAddons::OnPatchCreateClick(bool checked) {
    std::filesystem::path addon_path =
        Common::FS::GetSuyuPath(Common::FS::SuyuPath::LoadDir) / fmt::format("{:016X}", title_id);

    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromStdString("New Patch"));

    QFormLayout form(&dialog);
    form.addRow(
        new QLabel(QString::fromStdString("Enter the name of the patch that will be created")));

    QLineEdit* lineEdit = new QLineEdit(&dialog);
    form.addRow(QString::fromStdString("Patch Name"), lineEdit);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal,
                               &dialog);

    form.addRow(&buttonBox);
    QObject::connect(&buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(&buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));

    if (dialog.exec() == QDialog::Accepted) {
        std::filesystem::path addon_root_path = addon_path / lineEdit->text().toStdString();
        std::filesystem::path addon_exefs_path = addon_root_path / "exefs";
        std::filesystem::path addon_file_path = addon_exefs_path / "patch.pchtxt";

        // Create the folders
        if (!Common::FS::CreateDir(addon_root_path)) {
            LOG_ERROR(Core, "Could not create new addon root path at {}",
                      addon_root_path.generic_string());
            return;
        }

        if (!Common::FS::CreateDir(addon_exefs_path)) {
            LOG_ERROR(Core, "Could not create new addon root path at {}",
                      addon_root_path.generic_string());
            return;
        }

        // Create the patch file
        std::ofstream patch_file(addon_file_path);
        patch_file << "# Place your patches below" << std::endl;
        patch_file.close();

        // Reload everything
        ReloadList();
    }
}

void ConfigurePerGameAddons::OnPatchEditClick(bool checked) {
    if (!selected_patch || !selected_patch->file_path) {
        // Either no patch selected or selected patch somehow doesn't have a file?
        return;
    }

    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QString::fromStdString(selected_patch->file_path.value())));
}

void ConfigurePerGameAddons::OnPatchRemoveClick(bool checked) {
    if (!selected_patch || !selected_patch->file_path || !selected_patch->root_path) {
        // Either no patch selected or selected patch somehow doesn't have a file?
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        this, QString::fromStdString("Remove patch confirmation"),
        QString::fromStdString(
            "Are you sure you want to remove the patch '%1'? This action is permanent!")
            .arg(QString::fromStdString(selected_patch->name)),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    // Remove the patch then reload
    if (!Common::FS::RemoveDirRecursively(selected_patch->root_path.value_or("Invalid Path"))) {
        LOG_ERROR(Core, "Could not create new addon root path at {}",
                  selected_patch->root_path.value_or("Invalid Path"));
    }

    ReloadList();
}

void ConfigurePerGameAddons::OnPatchOpenFolder(bool checked) {
    std::filesystem::path path =
        Common::FS::GetSuyuPath(Common::FS::SuyuPath::LoadDir) / fmt::format("{:016X}", title_id);

    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path.generic_string())));
}
