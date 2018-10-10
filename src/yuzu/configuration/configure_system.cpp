// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QGraphicsItem>
#include <QList>
#include <QMessageBox>
#include <qinputdialog.h>
#include "common/common_paths.h"
#include "common/logging/backend.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_system.h"
#include "yuzu/configuration/configure_system.h"
#include "yuzu/main.h"

static const std::array<int, 12> days_in_month = {{
    31,
    29,
    31,
    30,
    31,
    30,
    31,
    31,
    30,
    31,
    30,
    31,
}};

// Same backup JPEG used by acc IProfile::GetImage if no jpeg found
static constexpr std::array<u8, 107> backup_jpeg{
    0xff, 0xd8, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x02, 0x02,
    0x02, 0x03, 0x03, 0x03, 0x03, 0x04, 0x06, 0x04, 0x04, 0x04, 0x04, 0x04, 0x08, 0x06, 0x06, 0x05,
    0x06, 0x09, 0x08, 0x0a, 0x0a, 0x09, 0x08, 0x09, 0x09, 0x0a, 0x0c, 0x0f, 0x0c, 0x0a, 0x0b, 0x0e,
    0x0b, 0x09, 0x09, 0x0d, 0x11, 0x0d, 0x0e, 0x0f, 0x10, 0x10, 0x11, 0x10, 0x0a, 0x0c, 0x12, 0x13,
    0x12, 0x10, 0x13, 0x0f, 0x10, 0x10, 0x10, 0xff, 0xc9, 0x00, 0x0b, 0x08, 0x00, 0x01, 0x00, 0x01,
    0x01, 0x01, 0x11, 0x00, 0xff, 0xcc, 0x00, 0x06, 0x00, 0x10, 0x10, 0x05, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x01, 0x00, 0x00, 0x3f, 0x00, 0xd2, 0xcf, 0x20, 0xff, 0xd9,
};

ConfigureSystem::ConfigureSystem(QWidget* parent) : QWidget(parent), ui(new Ui::ConfigureSystem) {
    ui->setupUi(this);
    connect(ui->combo_birthmonth,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &ConfigureSystem::updateBirthdayComboBox);
    connect(ui->button_regenerate_console_id, &QPushButton::clicked, this,
            &ConfigureSystem::refreshConsoleID);

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
    tree_view->setIconSize({64, 64});
    tree_view->setContextMenuPolicy(Qt::NoContextMenu);

    item_model->insertColumns(0, 1);
    item_model->setHeaderData(0, Qt::Horizontal, "Users");

    // We must register all custom types with the Qt Automoc system so that we are able to use it
    // with signals/slots. In this case, QList falls under the umbrells of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tree_view);

    ui->scrollArea->setLayout(layout);

    connect(tree_view, &QTreeView::clicked, this, &ConfigureSystem::SelectUser);

    connect(ui->pm_add, &QPushButton::pressed, this, &ConfigureSystem::AddUser);
    connect(ui->pm_rename, &QPushButton::pressed, this, &ConfigureSystem::RenameUser);
    connect(ui->pm_remove, &QPushButton::pressed, this, &ConfigureSystem::DeleteUser);

    scene = new QGraphicsScene;
    ui->current_user_icon->setScene(scene);

    this->setConfiguration();
}

ConfigureSystem::~ConfigureSystem() = default;

void ConfigureSystem::setConfiguration() {
    enabled = !Core::System::GetInstance().IsPoweredOn();

    ui->combo_language->setCurrentIndex(Settings::values.language_index);

    item_model->removeRows(0, item_model->rowCount());
    list_items.clear();

    std::transform(Settings::values.users.begin(), Settings::values.users.end(),
                   std::back_inserter(list_items),
                   [](const std::pair<std::string, Service::Account::UUID>& user) {
                       const auto icon_url = QString::fromStdString(
                           FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "users" +
                           DIR_SEP + user.first + ".jpg");
                       QPixmap icon{icon_url};

                       if (!icon) {
                           icon.fill(QColor::fromRgb(0, 0, 0));
                           icon.loadFromData(backup_jpeg.data(), backup_jpeg.size());
                       }

                       return QList{new QStandardItem{
                           icon.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                           QString::fromStdString(user.first + "\n" + user.second.Format())}};
                   });

    for (const auto& item : list_items)
        item_model->appendRow(item);

    UpdateCurrentUser();
}

void ConfigureSystem::UpdateCurrentUser() {
    const auto& current_user = Settings::values.users[Settings::values.current_user];
    const auto icon_url =
        QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "users" +
                               DIR_SEP + current_user.first + ".jpg");
    QPixmap icon{icon_url};

    if (!icon) {
        icon.fill(QColor::fromRgb(0, 0, 0));
        icon.loadFromData(backup_jpeg.data(), backup_jpeg.size());
    }

    scene->clear();
    scene->addPixmap(icon.scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    ui->current_user_username->setText(QString::fromStdString(current_user.first));
}

void ConfigureSystem::ReadSystemSettings() {}

void ConfigureSystem::applyConfiguration() {
    if (!enabled)
        return;

    Settings::values.language_index = ui->combo_language->currentIndex();
    Settings::Apply();
}

void ConfigureSystem::updateBirthdayComboBox(int birthmonth_index) {
    if (birthmonth_index < 0 || birthmonth_index >= 12)
        return;

    // store current day selection
    int birthday_index = ui->combo_birthday->currentIndex();

    // get number of days in the new selected month
    int days = days_in_month[birthmonth_index];

    // if the selected day is out of range,
    // reset it to 1st
    if (birthday_index < 0 || birthday_index >= days)
        birthday_index = 0;

    // update the day combo box
    ui->combo_birthday->clear();
    for (int i = 1; i <= days; ++i) {
        ui->combo_birthday->addItem(QString::number(i));
    }

    // restore the day selection
    ui->combo_birthday->setCurrentIndex(birthday_index);
}

void ConfigureSystem::refreshConsoleID() {
    QMessageBox::StandardButton reply;
    QString warning_text = tr("This will replace your current virtual Switch with a new one. "
                              "Your current virtual Switch will not be recoverable. "
                              "This might have unexpected effects in games. This might fail, "
                              "if you use an outdated config savegame. Continue?");
    reply = QMessageBox::critical(this, tr("Warning"), warning_text,
                                  QMessageBox::No | QMessageBox::Yes);
    if (reply == QMessageBox::No)
        return;
    u64 console_id{};
    ui->label_console_id->setText(
        tr("Console ID: 0x%1").arg(QString::number(console_id, 16).toUpper()));
}

void ConfigureSystem::SelectUser(const QModelIndex& index) {
    Settings::values.current_user =
        std::clamp<std::size_t>(index.row(), 0, Settings::values.users.size() - 1);

    UpdateCurrentUser();

    if (Settings::values.users.size() >= 2)
        ui->pm_remove->setEnabled(true);
    else
        ui->pm_remove->setEnabled(false);

    ui->pm_rename->setEnabled(true);
}

void ConfigureSystem::AddUser() {
    Service::Account::UUID uuid;
    uuid.Generate();

    bool ok = false;
    const auto username =
        QInputDialog::getText(this, tr("Enter Username"), tr("Enter a username for the new user:"),
                              QLineEdit::Normal, QString(), &ok);

    Settings::values.users.emplace_back(username.toStdString(), uuid);

    setConfiguration();
}

void ConfigureSystem::RenameUser() {
    const auto user = tree_view->currentIndex().row();

    bool ok = false;
    const auto new_username = QInputDialog::getText(
        this, tr("Enter Username"), tr("Enter a new username:"), QLineEdit::Normal,
        QString::fromStdString(Settings::values.users[user].first), &ok);

    if (!ok)
        return;

    Settings::values.users[user].first = new_username.toStdString();

    setConfiguration();
}

void ConfigureSystem::DeleteUser() {
    const auto user = Settings::values.users.begin() + tree_view->currentIndex().row();
    const auto confirm = QMessageBox::question(
        this, tr("Confirm Delete"),
        tr("You are about to delete user with name %1. Are you sure?").arg(user->first.c_str()));

    if (confirm == QMessageBox::No)
        return;

    if (Settings::values.current_user == tree_view->currentIndex().row())
        Settings::values.current_user = 0;

    Settings::values.users.erase(user);

    setConfiguration();

    ui->pm_remove->setEnabled(false);
    ui->pm_rename->setEnabled(false);
}
