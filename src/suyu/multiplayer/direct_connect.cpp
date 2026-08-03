// SPDX-FileCopyrightText: Copyright 2017 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QComboBox>
#include <QFuture>
#include <QInputDialog>
#include <QIntValidator>
#include <QRegularExpressionValidator>
#include <QString>
#include <QtConcurrent/QtConcurrentRun>
#include "common/settings.h"
#include "core/core.h"
#include "core/internal_network/network_interface.h"
#include "network/network.h"
#include "suyu/main.h"
#include "suyu/multiplayer/client_room.h"
#include "suyu/multiplayer/direct_connect.h"
#include "suyu/multiplayer/message.h"
#include "suyu/multiplayer/state.h"
#include "suyu/multiplayer/validation.h"
#include "suyu/uisettings.h"
#include "ui_direct_connect.h"

enum class ConnectionType : u8 { TraversalServer, IP };

DirectConnectWindow::DirectConnectWindow(Core::System& system_, QWidget* parent)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint),
      ui(std::make_unique<Ui::DirectConnect>()), system{system_}, room_network{
                                                                      system.GetRoomNetwork()} {

    ui->setupUi(this);

    // setup the watcher for background connections
    watcher = new QFutureWatcher<void>;
    connect(watcher, &QFutureWatcher<void>::finished, this, &DirectConnectWindow::OnConnection);

    ui->nickname->setValidator(validation.GetNickname());
    ui->nickname->setText(
        QString::fromStdString(UISettings::values.multiplayer_nickname.GetValue()));
    if (ui->nickname->text().isEmpty() && !Settings::values.suyu_username.GetValue().empty()) {
        // Use Eden Web Service user name as nickname by default
        ui->nickname->setText(QString::fromStdString(Settings::values.suyu_username.GetValue()));
    }
    ui->ip->setValidator(validation.GetIP());
    ui->ip->setText(QString::fromStdString(UISettings::values.multiplayer_ip.GetValue()));
    ui->port->setValidator(validation.GetPort());
    ui->port->setText(QString::number(UISettings::values.multiplayer_port.GetValue()));

    connect(ui->server_list, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DirectConnectWindow::OnServerSelected);
    connect(ui->add_server, &QPushButton::clicked, this, &DirectConnectWindow::OnAddServer);
    connect(ui->remove_server, &QPushButton::clicked, this, &DirectConnectWindow::OnRemoveServer);
    connect(ui->connect, &QPushButton::clicked, this, &DirectConnectWindow::Connect);

    LoadSavedServers();
}

DirectConnectWindow::~DirectConnectWindow() = default;

void DirectConnectWindow::RetranslateUi() {
    ui->retranslateUi(this);
}

void DirectConnectWindow::LoadSavedServers() {
    const QString saved = QString::fromStdString(UISettings::values.multiplayer_saved_servers.GetValue());
    const QStringList entries = saved.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    ui->server_list->clear();
    ui->server_list->addItem(tr("Custom Server"), QString());
    for (const QString& entry : entries) {
        QString host;
        QString port;
        if (!ParseSavedServerEntry(entry, host, port)) {
            continue;
        }
        const QString label = entry.left(entry.indexOf(QLatin1String("||")));
        const QString address = host + QLatin1Char(':') + port;
        ui->server_list->addItem(label, address);
    }
    ui->remove_server->setEnabled(false);
}

void DirectConnectWindow::SaveSavedServers() {
    QStringList entries;
    for (int i = 1; i < ui->server_list->count(); ++i) {
        const QString label = ui->server_list->itemText(i);
        const QString address = ui->server_list->itemData(i).toString();
        if (!label.isEmpty() && !address.isEmpty()) {
            entries.append(label + QStringLiteral("||") + address);
        }
    }
    UISettings::values.multiplayer_saved_servers = entries.join(QLatin1Char('\n')).toStdString();
    emit SaveConfig();
}

void DirectConnectWindow::UpdateSavedServerList() {
    LoadSavedServers();
}

bool DirectConnectWindow::ParseSavedServerEntry(const QString& entry, QString& host, QString& port) const {
    const int separator = entry.indexOf(QLatin1String("||"));
    if (separator < 0)
        return false;
    const QString address = entry.mid(separator + 2);
    const int colon = address.lastIndexOf(QLatin1Char(':'));
    if (colon <= 0 || colon == address.length() - 1)
        return false;
    host = address.left(colon);
    port = address.mid(colon + 1);
    return true;
}

void DirectConnectWindow::OnServerSelected(int index) {
    if (index <= 0) {
        ui->remove_server->setEnabled(false);
        return;
    }
    const QString address = ui->server_list->itemData(index).toString();
    ui->remove_server->setEnabled(!address.isEmpty());
    const int colon = address.lastIndexOf(QLatin1Char(':'));
    if (colon > 0) {
        ui->ip->setText(address.left(colon));
        ui->port->setText(address.mid(colon + 1));
    }
}

void DirectConnectWindow::OnAddServer() {
    bool ok;
    const QString label = QInputDialog::getText(this, tr("Add Custom Server"),
                                                tr("Server Name:"), QLineEdit::Normal,
                                                QString(), &ok);
    if (!ok || label.trimmed().isEmpty()) {
        return;
    }
    const QString address = QInputDialog::getText(this, tr("Add Custom Server"),
                                                  tr("Host:Port:"), QLineEdit::Normal,
                                                  QString(), &ok);
    if (!ok || address.trimmed().isEmpty()) {
        return;
    }
    const int colon = address.lastIndexOf(QLatin1Char(':'));
    if (colon <= 0 || colon == address.length() - 1) {
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::IP_ADDRESS_NOT_VALID);
        return;
    }
    ui->server_list->addItem(label.trimmed(), address.trimmed());
    ui->server_list->setCurrentIndex(ui->server_list->count() - 1);
    SaveSavedServers();
}

void DirectConnectWindow::OnRemoveServer() {
    const int index = ui->server_list->currentIndex();
    if (index <= 0) {
        return;
    }
    ui->server_list->removeItem(index);
    ui->remove_server->setEnabled(false);
    SaveSavedServers();
}

void DirectConnectWindow::Connect() {
    if (!Network::GetSelectedNetworkInterface()) {
        NetworkMessage::ErrorManager::ShowError(
            NetworkMessage::ErrorManager::NO_INTERFACE_SELECTED);
        return;
    }
    if (!ui->nickname->hasAcceptableInput()) {
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::USERNAME_NOT_VALID);
        return;
    }
    if (system.IsPoweredOn()) {
        if (!NetworkMessage::WarnGameRunning()) {
            return;
        }
    }
    if (const auto member = room_network.GetRoomMember().lock()) {
        // Prevent the user from trying to join a room while they are already joining.
        if (member->GetState() == Network::RoomMember::State::Joining) {
            return;
        } else if (member->IsConnected()) {
            // And ask if they want to leave the room if they are already in one.
            if (!NetworkMessage::WarnDisconnect()) {
                return;
            }
        }
    }
    if (!ui->ip->hasAcceptableInput()) {
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::IP_ADDRESS_NOT_VALID);
        return;
    }
    if (!ui->port->hasAcceptableInput()) {
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::PORT_NOT_VALID);
        return;
    }

    // Store settings
    UISettings::values.multiplayer_nickname = ui->nickname->text().toStdString();
    UISettings::values.multiplayer_ip = ui->ip->text().toStdString();
    if (!ui->port->text().isEmpty()) {
        UISettings::values.multiplayer_port = ui->port->text().toInt();
    } else {
        UISettings::values.multiplayer_port = UISettings::values.multiplayer_port.GetDefault();
    }

    emit SaveConfig();

    // attempt to connect in a different thread
    QFuture<void> f = QtConcurrent::run([&] {
        if (auto room_member = room_network.GetRoomMember().lock()) {
            auto port = UISettings::values.multiplayer_port.GetValue();
            room_member->Join(ui->nickname->text().toStdString(),
                              ui->ip->text().toStdString().c_str(), port, 0, Network::NoPreferredIP,
                              ui->password->text().toStdString().c_str());
        }
    });
    watcher->setFuture(f);
    // and disable widgets and display a connecting while we wait
    BeginConnecting();
}

void DirectConnectWindow::BeginConnecting() {
    ui->connect->setEnabled(false);
    ui->connect->setText(tr("Connecting"));
}

void DirectConnectWindow::EndConnecting() {
    ui->connect->setEnabled(true);
    ui->connect->setText(tr("Connect"));
}

void DirectConnectWindow::OnConnection() {
    EndConnecting();

    if (auto room_member = room_network.GetRoomMember().lock()) {
        if (room_member->IsConnected()) {
            close();
        }
    }
}
