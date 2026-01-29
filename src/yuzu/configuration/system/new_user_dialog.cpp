// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include "common/common_types.h"
#include "common/fs/path_util.h"
#include "common/uuid.h"
#include "configuration/system/profile_avatar_dialog.h"
#include "core/constants.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/romfs.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "new_user_dialog.h"
#include "qt_common/qt_common.h"
#include "ui_new_user_dialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QStyle>
#include <fmt/format.h>
#include <qnamespace.h>
#include <qregularexpression.h>
#include <qvalidator.h>

QPixmap NewUserDialog::DefaultAvatar() {
    QPixmap icon;

    icon.fill(Qt::black);
    icon.loadFromData(Core::Constants::ACCOUNT_BACKUP_JPEG.data(),
                      static_cast<u32>(Core::Constants::ACCOUNT_BACKUP_JPEG.size()));

    return icon.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QString NewUserDialog::GetImagePath(const Common::UUID& uuid) {
    const auto path =
        Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir) /
        fmt::format("system/save/8000000000000010/su/avators/{}.jpg", uuid.FormattedString());
    return QString::fromStdString(Common::FS::PathToUTF8String(path));
}

QPixmap NewUserDialog::GetIcon(const Common::UUID& uuid) {
    QPixmap icon{GetImagePath(uuid)};

    if (!icon) {
        return DefaultAvatar();
    }

    return icon.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

NewUserDialog::NewUserDialog(Common::UUID uuid, const std::string& username, const QString& title,
                             QWidget* parent)
    : QDialog(parent) {
    m_editing = true;
    setup(uuid, username, title);
}

NewUserDialog::NewUserDialog(QWidget* parent) : QDialog(parent) {
    setup(Common::UUID::MakeRandom(), "Eden", tr("New User"));
}

void NewUserDialog::setup(Common::UUID uuid, const std::string& username, const QString& title) {
    ui = new Ui::NewUserDialog;
    ui->setupUi(this);

    setWindowTitle(title);

    m_scene = new QGraphicsScene;
    ui->image->setScene(m_scene);

    // setup

    // Byte reversing here is incredibly wack, but basically the way I've implemented it
    // is such that when you view it, it will match the folder name always.
    // TODO: Add UUID changing
    if (!m_editing) {
        ui->uuid->setText(QString::fromStdString(uuid.RawString()).toUpper());
    } else {
        QByteArray bytes = QByteArray::fromHex(QString::fromStdString(uuid.RawString()).toLatin1());
        std::reverse(bytes.begin(), bytes.end());
        QString txt = QString::fromLatin1(bytes.toHex().toUpper());

        ui->uuid->setText(txt);
        ui->uuid->setReadOnly(true);
    }

    ui->username->setText(QString::fromStdString(username));
    verifyUser();

    setImage(GetIcon(uuid));
    updateRevertButton();

    // Validators
    QRegularExpressionValidator* username_validator =
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral(".{1,32}")), this);
    ui->username->setValidator(username_validator);

    QRegularExpressionValidator* uuid_validator = new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9a-fA-F]{32}")), this);
    ui->uuid->setValidator(uuid_validator);

    // Connections
    connect(ui->uuid, &QLineEdit::textEdited, this, &::NewUserDialog::verifyUser);
    connect(ui->username, &QLineEdit::textEdited, this, &::NewUserDialog::verifyUser);

    connect(ui->revert, &QAbstractButton::clicked, this, &NewUserDialog::revertImage);
    connect(ui->selectImage, &QAbstractButton::clicked, this, &NewUserDialog::selectImage);
    connect(ui->setAvatar, &QAbstractButton::clicked, this, &NewUserDialog::setAvatar);
    connect(ui->generate, &QAbstractButton::clicked, this, &NewUserDialog::generateUUID);

    connect(this, &NewUserDialog::isDefaultAvatarChanged, this, &NewUserDialog::updateRevertButton);
    connect(this, &QDialog::accepted, this, &NewUserDialog::dispatchUser);

    // dialog
    avatar_dialog = new ProfileAvatarDialog(this);
}

NewUserDialog::~NewUserDialog() {
    delete ui;
}

bool NewUserDialog::isDefaultAvatar() const {
    return m_isDefaultAvatar;
}

void NewUserDialog::setIsDefaultAvatar(bool newIsDefaultAvatar) {
    if (m_isDefaultAvatar == newIsDefaultAvatar)
        return;
    m_isDefaultAvatar = newIsDefaultAvatar;
    emit isDefaultAvatarChanged(m_isDefaultAvatar);
}

void NewUserDialog::selectImage() {
    const auto file = QFileDialog::getOpenFileName(this, tr("Select User Image"), QString(),
                                                   tr("Image Formats (*.jpg *.jpeg *.png *.bmp)"));
    if (file.isEmpty()) {
        return;
    }

    // Profile image must be 256x256
    QPixmap image(file);
    if (image.width() != 256 || image.height() != 256) {
        image = image.scaled(256, 256, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }

    setImage(image.scaled(64, 64, Qt::IgnoreAspectRatio));
    setIsDefaultAvatar(false);
}

void NewUserDialog::setAvatar() {
    if (!avatar_dialog->AreImagesLoaded()) {
        if (!LoadAvatarData()) {
            return;
        }
    }
    if (avatar_dialog->exec() == QDialog::Accepted) {
        setImage(avatar_dialog->GetSelectedAvatar().scaled(64, 64, Qt::IgnoreAspectRatio));
    }
}

bool NewUserDialog::LoadAvatarData() {
    constexpr u64 AvatarImageDataId = 0x010000000000080AULL;

    // Attempt to load avatar data archive from installed firmware
    auto* bis_system = QtCommon::system->GetFileSystemController().GetSystemNANDContents();
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
        QMessageBox::warning(
            this, tr("Error loading archive"),
            tr("Could not locate RomFS. Your file or decryption keys may be corrupted."));
        return false;
    }
    const auto extracted = FileSys::ExtractRomFS(romfs);
    if (!extracted) {
        QMessageBox::warning(
            this, tr("Error extracting archive"),
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

std::vector<uint8_t> NewUserDialog::DecompressYaz0(const FileSys::VirtualFile& file) {
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

void NewUserDialog::setImage(const QPixmap& pixmap) {
    m_pixmap = pixmap;
    m_scene->clear();
    m_scene->addPixmap(m_pixmap);
}

void NewUserDialog::revertImage() {
    setImage(DefaultAvatar());
    setIsDefaultAvatar(true);
}

void NewUserDialog::updateRevertButton() {
    if (isDefaultAvatar()) {
        ui->revert->setIcon(QIcon{});
    } else {
        QStyle* style = parentWidget()->style();
        QIcon icon(style->standardIcon(QStyle::SP_LineEditClearButton));
        ui->revert->setIcon(icon);
    }
}

void NewUserDialog::generateUUID() {
    Common::UUID uuid = Common::UUID::MakeRandom();
    ui->uuid->setText(QString::fromStdString(uuid.RawString()).toUpper());
}

void NewUserDialog::verifyUser() {
    const QPixmap checked = QIcon::fromTheme(QStringLiteral("checked")).pixmap(16);
    const QPixmap failed = QIcon::fromTheme(QStringLiteral("failed")).pixmap(16);

    const bool uuid_good = ui->uuid->hasAcceptableInput();
    const bool username_good = ui->username->hasAcceptableInput();

    auto ok_button = ui->buttonBox->button(QDialogButtonBox::Ok);
    ok_button->setEnabled(username_good && uuid_good);

    if (uuid_good) {
        ui->uuidVerified->setPixmap(checked);
        ui->uuidVerified->setToolTip(tr("All Good", "Tooltip"));
    } else {
        ui->uuidVerified->setPixmap(failed);
        ui->uuidVerified->setToolTip(tr("Must be 32 hex characters (0-9, a-f)", "Tooltip"));
    }

    if (username_good) {
        ui->usernameVerified->setPixmap(checked);
        ui->usernameVerified->setToolTip(tr("All Good", "Tooltip"));
    } else {
        ui->usernameVerified->setPixmap(failed);
        ui->usernameVerified->setToolTip(tr("Must be between 1 and 32 characters", "Tooltip"));
    }
}

// TODO: Move UUID
void NewUserDialog::dispatchUser() {
    QByteArray bytes = QByteArray::fromHex(ui->uuid->text().toLatin1());

    // convert to 16 u8's
    std::array<u8, 16> uuid_arr;
    std::copy_n(reinterpret_cast<const u8*>(bytes.constData()), 16, uuid_arr.begin());

    if (!m_editing) {
        std::ranges::reverse(uuid_arr);
    }

    Common::UUID uuid(uuid_arr);

    User user{
        .username = ui->username->text(),
        .uuid = uuid,
        .pixmap = m_pixmap,
    };

    emit userAdded(user);
}
