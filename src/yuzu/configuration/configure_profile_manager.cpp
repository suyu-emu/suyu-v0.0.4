// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <functional>
#include <iterator>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGraphicsItem>
#include <QHeaderView>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QTreeView>
#include <qnamespace.h>
#include <qtreeview.h>

#include "common/assert.h"
#include "common/fs/path_util.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "configuration/system/new_user_dialog.h"
#include "core/constants.h"
#include "core/core.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "ui_configure_profile_manager.h"
#include "yuzu/configuration/configure_profile_manager.h"
#include "yuzu/util/limitable_input_dialog.h"

namespace {

QString GetImagePath(const Common::UUID& uuid) {
    const auto path =
        Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir) /
        fmt::format("system/save/8000000000000010/su/avators/{}.jpg", uuid.FormattedString());
    return QString::fromStdString(Common::FS::PathToUTF8String(path));
}

QString GetAccountUsername(const Service::Account::ProfileManager& manager, Common::UUID uuid) {
    Service::Account::ProfileBase profile{};
    if (!manager.GetProfileBase(uuid, profile)) {
        return {};
    }

    const auto text = Common::StringFromFixedZeroTerminatedBuffer(
        reinterpret_cast<const char*>(profile.username.data()), profile.username.size());
    return QString::fromStdString(text);
}

QString FormatUserEntryText(const QString& username, Common::UUID uuid) {
    return ConfigureProfileManager::tr("%1\n%2",
                                       "%1 is the profile username, %2 is the formatted UUID (e.g. "
                                       "00112233-4455-6677-8899-AABBCCDDEEFF))")
        .arg(username, QString::fromStdString(uuid.FormattedString()));
}

QPixmap GetIcon(const Common::UUID& uuid) {
    QPixmap icon{GetImagePath(uuid)};

    if (!icon) {
        icon.fill(Qt::black);
        icon.loadFromData(Core::Constants::ACCOUNT_BACKUP_JPEG.data(),
                          static_cast<u32>(Core::Constants::ACCOUNT_BACKUP_JPEG.size()));
    }

    return icon.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // Anonymous namespace

ConfigureProfileManager::ConfigureProfileManager(Core::System& system_, QWidget* parent)
    : QWidget(parent), ui{std::make_unique<Ui::ConfigureProfileManager>()},
      profile_manager{system_.GetProfileManager()}, system{system_} {
    ui->setupUi(this);

    tree_view = new QTreeView;
    item_model = new QStandardItemModel(tree_view);
    item_model->insertColumns(0, 1);
    tree_view->setModel(item_model);
    tree_view->setAlternatingRowColors(true);
    tree_view->setSelectionMode(QHeaderView::SingleSelection);
    tree_view->setSelectionBehavior(QHeaderView::SelectRows);
    tree_view->setVerticalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setHorizontalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setSortingEnabled(true);
    tree_view->setEditTriggers(QHeaderView::NoEditTriggers);
    tree_view->setUniformRowHeights(true);
    tree_view->setIconSize({64, 64});
    tree_view->setContextMenuPolicy(Qt::CustomContextMenu);

    // We must register all custom types with the Qt Automoc system so that we are able to use it
    // with signals/slots. In this case, QList falls under the umbrells of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tree_view);

    ui->scrollArea->setLayout(layout);

    connect(tree_view, &QTreeView::customContextMenuRequested, this, &ConfigureProfileManager::showContextMenu);

    connect(tree_view, &QTreeView::clicked, this, &ConfigureProfileManager::SelectUser);

    connect(ui->pm_add, &QPushButton::clicked, this, &ConfigureProfileManager::AddUser);

    confirm_dialog = new ConfigureProfileManagerDeleteDialog(this);
    connect(confirm_dialog, &ConfigureProfileManagerDeleteDialog::deleteUser, this,
            &ConfigureProfileManager::DeleteUser);

    scene = new QGraphicsScene;
    ui->current_user_icon->setScene(scene);

    RetranslateUI();
    SetConfiguration();
}

ConfigureProfileManager::~ConfigureProfileManager() = default;

void ConfigureProfileManager::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureProfileManager::RetranslateUI() {
    ui->retranslateUi(this);
    item_model->setHeaderData(0, Qt::Horizontal, tr("Users"));
}

void ConfigureProfileManager::SetConfiguration() {
    enabled = !system.IsPoweredOn();
    item_model->removeRows(0, item_model->rowCount());
    list_items.clear();

    PopulateUserList();
    UpdateCurrentUser();
}

void ConfigureProfileManager::PopulateUserList() {
    profile_manager.ResetUserSaveFile();
    const auto& profiles = profile_manager.GetAllUsers();
    for (const auto& user : profiles) {
        Service::Account::ProfileBase profile{};
        if (!profile_manager.GetProfileBase(user, profile)) {
            continue;
        }

        const auto username = Common::StringFromFixedZeroTerminatedBuffer(
            reinterpret_cast<const char*>(profile.username.data()), profile.username.size());

        list_items.push_back(QList<QStandardItem*>{new QStandardItem{
            GetIcon(user), FormatUserEntryText(QString::fromStdString(username), user)}});
    }

    for (const auto& item : list_items)
        item_model->appendRow(item);
}

void ConfigureProfileManager::UpdateCurrentUser() {
    ui->pm_add->setEnabled(profile_manager.GetUserCount() < Service::Account::MAX_USERS);

    const auto& current_user = profile_manager.GetUser(Settings::values.current_user.GetValue());
    ASSERT(current_user);
    const auto username = GetAccountUsername(profile_manager, *current_user);

    scene->clear();
    scene->addPixmap(
        GetIcon(*current_user).scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    ui->current_user_username->setText(username);
}

void ConfigureProfileManager::ApplyConfiguration() {
    if (!enabled) {
        return;
    }
}

void ConfigureProfileManager::saveImage(QPixmap pixmap, Common::UUID uuid) {
    const auto image_path = GetImagePath(uuid);
    if (QFile::exists(image_path) && !QFile::remove(image_path)) {
        QMessageBox::warning(
            this, tr("Error deleting image"),
            tr("Error occurred attempting to overwrite previous image at: %1.").arg(image_path));
        return;
    }

    const auto raw_path = QString::fromStdString(Common::FS::PathToUTF8String(
        Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir) / "system/save/8000000000000010"));
    const QFileInfo raw_info{raw_path};
    if (raw_info.exists() && !raw_info.isDir() && !QFile::remove(raw_path)) {
        QMessageBox::warning(this, tr("Error deleting file"),
                             tr("Unable to delete existing file: %1.").arg(raw_path));
        return;
    }

    const QString absolute_dst_path = QFileInfo{image_path}.absolutePath();
    if (!QDir{raw_path}.mkpath(absolute_dst_path)) {
        QMessageBox::warning(
            this, tr("Error creating user image directory"),
            tr("Unable to create directory %1 for storing user images.").arg(absolute_dst_path));
        return;
    }

    if (!pixmap.save(image_path, "JPEG", 100)) {
        QMessageBox::warning(this, tr("Error saving user image"),
                             tr("Unable to save image to file"));
        return;
    }
}

void ConfigureProfileManager::showContextMenu(const QPoint& pos) {
    const QModelIndex index = tree_view->indexAt(pos);
    if (!index.isValid())
        return;

    QMenu menu(this);

    QAction* edit = menu.addAction(tr("&Edit"));
    QAction* remove = menu.addAction(tr("&Delete"));

    QAction* chosen =
        menu.exec(tree_view->viewport()->mapToGlobal(pos));

    if (!chosen)
        return;

    if (chosen == edit) {
        EditUser();
    } else if (chosen == remove) {
        ConfirmDeleteUser();
    }
}

void ConfigureProfileManager::SelectUser(const QModelIndex& index) {
    Settings::values.current_user =
        std::clamp<s32>(index.row(), 0, static_cast<s32>(profile_manager.GetUserCount() - 1));

    UpdateCurrentUser();
}

void ConfigureProfileManager::AddUser() {
    NewUserDialog *dialog = new NewUserDialog(this);

    connect(dialog, &NewUserDialog::userAdded, this, [dialog, this](User user) {
        auto uuid = user.uuid;
        auto username = user.username;
        auto pixmap = user.pixmap;

        profile_manager.CreateNewUser(uuid, username.toStdString());
        profile_manager.WriteUserSaveFile();
        item_model->appendRow(new QStandardItem{pixmap, FormatUserEntryText(username, uuid)});

        saveImage(pixmap, uuid);
        UpdateCurrentUser();

        dialog->deleteLater();
    });

    connect(dialog, &QDialog::rejected, dialog, &QObject::deleteLater);

    dialog->show();
}

void ConfigureProfileManager::EditUser() {
    const auto user_data = tree_view->currentIndex();
    const auto user_idx = user_data.row();
    const auto uuid = profile_manager.GetUser(user_idx);
    ASSERT(uuid);

    Service::Account::ProfileBase profile{};
    if (!profile_manager.GetProfileBase(*uuid, profile))
        return;

    std::string username;
    username.reserve(32);

    std::ranges::copy_if(profile.username, std::back_inserter(username), [](u8 byte) { return byte != 0; });

    NewUserDialog *dialog = new NewUserDialog(uuid.value(), username, tr("Edit User"), this);

    connect(dialog, &NewUserDialog::userAdded, this, [dialog, profile, user_idx, uuid, this](User user) mutable {
        // TODO: MOVE UUID
        // auto new_uuid = user.uuid;
        auto new_username = user.username;
        auto pixmap = user.pixmap;

        auto const uuid_val = uuid.value();

        const auto username_std = new_username.toStdString();
        std::fill(profile.username.begin(), profile.username.end(), '\0');
        std::copy(username_std.begin(), username_std.end(), profile.username.begin());

        profile_manager.SetProfileBase(uuid_val, profile);
        profile_manager.WriteUserSaveFile();

        item_model->setItem(
            user_idx, 0,
            new QStandardItem{pixmap,
                              FormatUserEntryText(QString::fromStdString(username_std), uuid_val)});

        saveImage(pixmap, uuid_val);
        UpdateCurrentUser();

        dialog->deleteLater();
    });

    connect(dialog, &QDialog::rejected, dialog, &QObject::deleteLater);

    dialog->show();
}

void ConfigureProfileManager::ConfirmDeleteUser() {
    const auto index = tree_view->currentIndex().row();
    const auto uuid = profile_manager.GetUser(index);
    ASSERT(uuid);
    const auto username = GetAccountUsername(profile_manager, *uuid);

    confirm_dialog->SetInfo(username, *uuid, index);
    confirm_dialog->show();
}

void ConfigureProfileManager::DeleteUser(const int index) {
    if (Settings::values.current_user.GetValue() == tree_view->currentIndex().row()) {
        Settings::values.current_user = 0;
    }

    if (!profile_manager.RemoveProfileAtIndex(index)) {
        return;
    }

    profile_manager.WriteUserSaveFile();
    profile_manager.ResetUserSaveFile();

    UpdateCurrentUser();

    item_model->removeRows(tree_view->currentIndex().row(), 1);
    tree_view->clearSelection();
}

ConfigureProfileManagerDeleteDialog::ConfigureProfileManagerDeleteDialog(QWidget* parent)
    : QDialog{parent} {
    auto dialog_vbox_layout = new QVBoxLayout(this);
    dialog_button_box =
        new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::No, Qt::Horizontal, parent);
    auto label_message =
        new QLabel(tr("Delete this user? All of the user's save data will be deleted."), this);
    label_info = new QLabel(this);
    auto dialog_hbox_layout_widget = new QWidget(this);
    auto dialog_hbox_layout = new QHBoxLayout(dialog_hbox_layout_widget);
    icon_scene = new QGraphicsScene(0, 0, 64, 64, this);
    auto icon_view = new QGraphicsView(icon_scene, this);

    dialog_hbox_layout_widget->setLayout(dialog_hbox_layout);
    icon_view->setMinimumSize(64, 64);
    icon_view->setMaximumSize(64, 64);
    icon_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    icon_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    icon_view->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    this->setLayout(dialog_vbox_layout);
    this->setWindowTitle(tr("Confirm Delete"));
    this->setSizeGripEnabled(false);

    dialog_vbox_layout->addWidget(label_message);
    dialog_vbox_layout->addWidget(dialog_hbox_layout_widget);
    dialog_vbox_layout->addWidget(dialog_button_box);
    dialog_hbox_layout->addWidget(icon_view);
    dialog_hbox_layout->addWidget(label_info);

    setMinimumSize(380, 160);

    connect(dialog_button_box, &QDialogButtonBox::rejected, this, [this]() { close(); });
    connect(dialog_button_box, &QDialogButtonBox::accepted, this, [this]() {
        close();
        emit deleteUser(m_index);
    });
}

ConfigureProfileManagerDeleteDialog::~ConfigureProfileManagerDeleteDialog() = default;

void ConfigureProfileManagerDeleteDialog::SetInfo(const QString& username, const Common::UUID& uuid,
                                                  int index) {
    label_info->setText(
        tr("Name: %1\nUUID: %2").arg(username, QString::fromStdString(uuid.FormattedString())));
    icon_scene->clear();
    icon_scene->addPixmap(GetIcon(uuid));

    m_index = index;
}
