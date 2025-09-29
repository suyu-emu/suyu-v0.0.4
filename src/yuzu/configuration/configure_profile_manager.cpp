// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <functional>
#include <iostream>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGraphicsItem>
#include <QHeaderView>
#include <QListWidget>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QTreeView>

#include "common/assert.h"
#include "common/fs/path_util.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "ui_configure_profile_manager.h"
#include "yuzu/util/limitable_input_dialog.h"
#include "yuzu/configuration/configure_profile_manager.h"

namespace {
// Same backup JPEG used by acc IProfile::GetImage if no jpeg found
constexpr std::array<u8, 107> backup_jpeg{
    0xff, 0xd8, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x02, 0x02,
    0x02, 0x03, 0x03, 0x03, 0x03, 0x04, 0x06, 0x04, 0x04, 0x04, 0x04, 0x04, 0x08, 0x06, 0x06, 0x05,
    0x06, 0x09, 0x08, 0x0a, 0x0a, 0x09, 0x08, 0x09, 0x09, 0x0a, 0x0c, 0x0f, 0x0c, 0x0a, 0x0b, 0x0e,
    0x0b, 0x09, 0x09, 0x0d, 0x11, 0x0d, 0x0e, 0x0f, 0x10, 0x10, 0x11, 0x10, 0x0a, 0x0c, 0x12, 0x13,
    0x12, 0x10, 0x13, 0x0f, 0x10, 0x10, 0x10, 0xff, 0xc9, 0x00, 0x0b, 0x08, 0x00, 0x01, 0x00, 0x01,
    0x01, 0x01, 0x11, 0x00, 0xff, 0xcc, 0x00, 0x06, 0x00, 0x10, 0x10, 0x05, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x01, 0x00, 0x00, 0x3f, 0x00, 0xd2, 0xcf, 0x20, 0xff, 0xd9,
};

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
        icon.loadFromData(backup_jpeg.data(), static_cast<u32>(backup_jpeg.size()));
    }

    return icon.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QString GetProfileUsernameFromUser(QWidget* parent, const QString& description_text) {
    return LimitableInputDialog::GetText(parent, ConfigureProfileManager::tr("Enter Username"),
                                         description_text, 1,
                                         static_cast<int>(Service::Account::profile_username_size));
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
    tree_view->setContextMenuPolicy(Qt::NoContextMenu);

    // We must register all custom types with the Qt Automoc system so that we are able to use it
    // with signals/slots. In this case, QList falls under the umbrells of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tree_view);

    ui->scrollArea->setLayout(layout);

    connect(tree_view, &QTreeView::clicked, this, &ConfigureProfileManager::SelectUser);

    connect(ui->pm_add, &QPushButton::clicked, this, &ConfigureProfileManager::AddUser);
    connect(ui->pm_rename, &QPushButton::clicked, this, &ConfigureProfileManager::RenameUser);
    connect(ui->pm_remove, &QPushButton::clicked, this,
            &ConfigureProfileManager::ConfirmDeleteUser);
    connect(ui->pm_set_image, &QPushButton::clicked, this,
            &ConfigureProfileManager::SelectImageFile);
    connect(ui->pm_select_avatar, &QPushButton::clicked, this,
            &ConfigureProfileManager::SelectFirmwareAvatar);

    avatar_dialog = new ConfigureProfileManagerAvatarDialog(this);
    confirm_dialog = new ConfigureProfileManagerDeleteDialog(this);

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
    const auto& profiles = profile_manager.GetAllUsers();
    for (const auto& user : profiles) {
        Service::Account::ProfileBase profile{};
        if (!profile_manager.GetProfileBase(user, profile))
            continue;

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
        GetIcon(*current_user).scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    ui->current_user_username->setText(username);
}

void ConfigureProfileManager::ApplyConfiguration() {
    if (!enabled) {
        return;
    }
}

void ConfigureProfileManager::SelectUser(const QModelIndex& index) {
    Settings::values.current_user =
        std::clamp<s32>(index.row(), 0, static_cast<s32>(profile_manager.GetUserCount() - 1));

    UpdateCurrentUser();

    ui->pm_remove->setEnabled(profile_manager.GetUserCount() >= 2);
    ui->pm_rename->setEnabled(true);
    ui->pm_set_image->setEnabled(true);
    ui->pm_select_avatar->setEnabled(true);
}

void ConfigureProfileManager::AddUser() {
    const auto username =
        GetProfileUsernameFromUser(this, tr("Enter a username for the new user:"));
    if (username.isEmpty()) {
        return;
    }

    const auto uuid = Common::UUID::MakeRandom();
    profile_manager.CreateNewUser(uuid, username.toStdString());
    profile_manager.WriteUserSaveFile();

    item_model->appendRow(new QStandardItem{GetIcon(uuid), FormatUserEntryText(username, uuid)});
}

void ConfigureProfileManager::RenameUser() {
    const auto user = tree_view->currentIndex().row();
    const auto uuid = profile_manager.GetUser(user);
    ASSERT(uuid);

    Service::Account::ProfileBase profile{};
    if (!profile_manager.GetProfileBase(*uuid, profile))
        return;

    const auto new_username = GetProfileUsernameFromUser(this, tr("Enter a new username:"));
    if (new_username.isEmpty()) {
        return;
    }

    const auto username_std = new_username.toStdString();
    std::fill(profile.username.begin(), profile.username.end(), '\0');
    std::copy(username_std.begin(), username_std.end(), profile.username.begin());

    profile_manager.SetProfileBase(*uuid, profile);
    profile_manager.WriteUserSaveFile();

    item_model->setItem(
        user, 0,
        new QStandardItem{GetIcon(*uuid),
                          FormatUserEntryText(QString::fromStdString(username_std), *uuid)});
    UpdateCurrentUser();
}

void ConfigureProfileManager::ConfirmDeleteUser() {
    const auto index = tree_view->currentIndex().row();
    const auto uuid = profile_manager.GetUser(index);
    ASSERT(uuid);
    const auto username = GetAccountUsername(profile_manager, *uuid);

    confirm_dialog->SetInfo(username, *uuid, [this, uuid]() { DeleteUser(*uuid); });
    confirm_dialog->show();
}

void ConfigureProfileManager::DeleteUser(const Common::UUID& uuid) {
    if (Settings::values.current_user.GetValue() == tree_view->currentIndex().row()) {
        Settings::values.current_user = 0;
    }
    UpdateCurrentUser();

    if (!profile_manager.RemoveUser(uuid)) {
        return;
    }

    profile_manager.WriteUserSaveFile();

    item_model->removeRows(tree_view->currentIndex().row(), 1);
    tree_view->clearSelection();

    ui->pm_remove->setEnabled(false);
    ui->pm_rename->setEnabled(false);
}

void ConfigureProfileManager::SetUserImage(const QImage& image) {
    const auto index = tree_view->currentIndex().row();
    const auto uuid = profile_manager.GetUser(index);
    ASSERT(uuid);

    const auto image_path = GetImagePath(*uuid);
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

    if (!image.save(image_path, "JPEG", 100)) {
        QMessageBox::warning(this, tr("Error saving user image"),
                             tr("Unable to save image to file"));
        return;
    }

    const auto username = GetAccountUsername(profile_manager, *uuid);
    item_model->setItem(index, 0,
                        new QStandardItem{GetIcon(*uuid), FormatUserEntryText(username, *uuid)});
    UpdateCurrentUser();
}

void ConfigureProfileManager::SelectImageFile() {
    const auto file = QFileDialog::getOpenFileName(this, tr("Select User Image"), QString(),
                                                   tr("Image Formats (*.jpg *.jpeg *.png *.bmp)"));
    if (file.isEmpty()) {
        return;
    }

    // Profile image must be 256x256
    QImage image(file);
    if (image.width() != 256 || image.height() != 256) {
        image = image.scaled(256, 256, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }
    SetUserImage(image);
}

void ConfigureProfileManager::SelectFirmwareAvatar() {
    if (!avatar_dialog->AreImagesLoaded()) {
        if (!LoadAvatarData()) {
            return;
        }
    }
    if (avatar_dialog->exec() == QDialog::Accepted) {
        SetUserImage(avatar_dialog->GetSelectedAvatar().toImage());
    }
}

bool ConfigureProfileManager::LoadAvatarData() {
    constexpr u64 AvatarImageDataId = 0x010000000000080AULL;

    // Attempt to load avatar data archive from installed firmware
    auto* bis_system = system.GetFileSystemController().GetSystemNANDContents();
    if (!bis_system) {
        QMessageBox::warning(this, tr("No firmware available"),
                             tr("Please install the firmware to use firmware avatars."));
        return false;
    }
    const auto nca = bis_system->GetEntry(AvatarImageDataId, FileSys::ContentRecordType::Data);
    if (!nca) {
        QMessageBox::warning(this, tr("Error loading archive"),
                             tr("Archive is not available. Please install/reinstall firmware."));
        return false;
    }
    const auto romfs = nca->GetRomFS();
    if (!romfs) {
        QMessageBox::warning(this, tr("Error loading archive"),
                             tr("Could not locate RomFS. Your file or decryption keys may be corrupted."));
        return false;
    }
    const auto extracted = FileSys::ExtractRomFS(romfs);
    if (!extracted) {
        QMessageBox::warning(this, tr("Error extracting archive"),
                             tr("Could not extract RomFS. Your file or decryption keys may be corrupted."));
        return false;
    }
    const auto chara_dir = extracted->GetSubdirectory("chara");
    if (!chara_dir) {
        QMessageBox::warning(this, tr("Error finding image directory"),
                             tr("Failed to find image directory in the archive."));
        return false;
    }

    QVector<QPixmap> images;
    for (const auto& item : chara_dir->GetFiles()) {
        if (item->GetExtension() != "szs") {
            continue;
        }

        auto image_data = DecompressYaz0(item);
        if (image_data.empty()) {
            continue;
        }
        QImage image(reinterpret_cast<const uchar*>(image_data.data()), 256, 256,
                     QImage::Format_RGBA8888);
        images.append(QPixmap::fromImage(image));
    }

    if (images.isEmpty()) {
        QMessageBox::warning(this, tr("No images found"),
                             tr("No avatar images were found in the archive."));
        return false;
    }

    // Load the image data into the dialog
    avatar_dialog->LoadImages(images);
    return true;
}

ConfigureProfileManagerAvatarDialog::ConfigureProfileManagerAvatarDialog(QWidget* parent)
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

ConfigureProfileManagerAvatarDialog::~ConfigureProfileManagerAvatarDialog() = default;

void ConfigureProfileManagerAvatarDialog::SetBackgroundColor(const QColor& color) {
    avatar_bg_color = color;

    bg_color_button->setStyleSheet(
        QStringLiteral("background-color: %1; min-width: 60px;").arg(avatar_bg_color.name()));
}

QPixmap ConfigureProfileManagerAvatarDialog::CreateAvatar(const QPixmap& avatar) {
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

void ConfigureProfileManagerAvatarDialog::RefreshAvatars() {
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

void ConfigureProfileManagerAvatarDialog::LoadImages(const QVector<QPixmap>& avatar_images) {
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

bool ConfigureProfileManagerAvatarDialog::AreImagesLoaded() const {
    return !avatar_image_store.isEmpty();
}

QPixmap ConfigureProfileManagerAvatarDialog::GetSelectedAvatar() {
    return CreateAvatar(avatar_image_store[avatar_list->currentRow()]);
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
    icon_view->setMaximumSize(64, 64);
    icon_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    icon_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setLayout(dialog_vbox_layout);
    this->setWindowTitle(tr("Confirm Delete"));
    this->setSizeGripEnabled(false);
    dialog_vbox_layout->addWidget(label_message);
    dialog_vbox_layout->addWidget(dialog_hbox_layout_widget);
    dialog_vbox_layout->addWidget(dialog_button_box);
    dialog_hbox_layout->addWidget(icon_view);
    dialog_hbox_layout->addWidget(label_info);

    connect(dialog_button_box, &QDialogButtonBox::rejected, this, [this]() { close(); });
}

ConfigureProfileManagerDeleteDialog::~ConfigureProfileManagerDeleteDialog() = default;

void ConfigureProfileManagerDeleteDialog::SetInfo(const QString& username, const Common::UUID& uuid,
                                                  std::function<void()> accept_callback) {
    label_info->setText(
        tr("Name: %1\nUUID: %2").arg(username, QString::fromStdString(uuid.FormattedString())));
    icon_scene->clear();
    icon_scene->addPixmap(GetIcon(uuid));

    connect(dialog_button_box, &QDialogButtonBox::accepted, this, [this, accept_callback]() {
        close();
        accept_callback();
    });
}

std::vector<uint8_t> ConfigureProfileManager::DecompressYaz0(const FileSys::VirtualFile& file) {
    if (!file) {
        throw std::invalid_argument("Null file pointer passed to DecompressYaz0");
    }

    uint32_t magic{};
    file->ReadObject(&magic, 0);
    if (magic != Common::MakeMagic('Y', 'a', 'z', '0')) {
        return std::vector<uint8_t>();
    }

    uint32_t decoded_length{};
    file->ReadObject(&decoded_length, 4);
    decoded_length = Common::swap32(decoded_length);

    std::size_t input_size = file->GetSize() - 16;
    std::vector<uint8_t> input(input_size);
    file->ReadBytes(input.data(), input_size, 16);

    uint32_t input_offset{};
    uint32_t output_offset{};
    std::vector<uint8_t> output(decoded_length);

    uint16_t mask{};
    uint8_t header{};

    while (output_offset < decoded_length) {
        if ((mask >>= 1) == 0) {
            header = input[input_offset++];
            mask = 0x80;
        }

        if ((header & mask) != 0) {
            if (output_offset == output.size()) {
                break;
            }
            output[output_offset++] = input[input_offset++];
        } else {
            uint8_t byte1 = input[input_offset++];
            uint8_t byte2 = input[input_offset++];

            uint32_t dist = ((byte1 & 0xF) << 8) | byte2;
            uint32_t position = output_offset - (dist + 1);

            uint32_t length = byte1 >> 4;
            if (length == 0) {
                length = static_cast<uint32_t>(input[input_offset++]) + 0x12;
            } else {
                length += 2;
            }

            uint32_t gap = output_offset - position;
            uint32_t non_overlapping_length = length;

            if (non_overlapping_length > gap) {
                non_overlapping_length = gap;
            }

            std::memcpy(&output[output_offset], &output[position], non_overlapping_length);
            output_offset += non_overlapping_length;
            position += non_overlapping_length;
            length -= non_overlapping_length;

            while (length-- > 0) {
                output[output_offset++] = output[position++];
            }
        }
    }

    return output;
}
