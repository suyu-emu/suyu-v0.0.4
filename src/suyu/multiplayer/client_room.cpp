// SPDX-FileCopyrightText: Copyright 2017 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <future>
#include <QColor>
#include <QImage>
#include <QList>
#include <QLocale>
#include <QMetaType>
#include <QMessageBox>
#include <QTime>
#include <QtConcurrent/QtConcurrentRun>
#include "common/logging/log.h"
#include "network/announce_multiplayer_session.h"
#include "suyu/game_list_p.h"
#include "suyu/multiplayer/client_room.h"
#include "suyu/multiplayer/message.h"
#include "suyu/multiplayer/moderation_dialog.h"
#include "suyu/multiplayer/state.h"
#include "ui_client_room.h"

ClientRoomWindow::ClientRoomWindow(QWidget* parent, Network::RoomNetwork& room_network_)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint),
      ui(std::make_unique<Ui::ClientRoom>()), room_network{room_network_} {
    ui->setupUi(this);
    ui->chat->Initialize(&room_network);

    // setup the callbacks for network updates
    if (auto member = room_network.GetRoomMember().lock()) {
        member->BindOnRoomInformationChanged(
            [this](const Network::RoomInformation& info) { emit RoomInformationChanged(info); });
        member->BindOnStateChanged(
            [this](const Network::RoomMember::State& state) { emit StateChanged(state); });

        connect(this, &ClientRoomWindow::RoomInformationChanged, this,
                &ClientRoomWindow::OnRoomUpdate);
        connect(this, &ClientRoomWindow::StateChanged, this, &::ClientRoomWindow::OnStateChange);
        // Update the state
        OnStateChange(member->GetState());
    } else {
        // TODO (jroweboy) network was not initialized?
    }

    connect(ui->disconnect, &QPushButton::clicked, this, &ClientRoomWindow::Disconnect);
    ui->disconnect->setDefault(false);
    ui->disconnect->setAutoDefault(false);
    connect(ui->launch_preferred_game, &QPushButton::clicked, this,
            &ClientRoomWindow::LaunchPreferredGame);
    ui->launch_preferred_game->setDefault(false);
    ui->launch_preferred_game->setAutoDefault(false);
    connect(ui->moderation, &QPushButton::clicked, [this] {
        ModerationDialog dialog(room_network, this);
        dialog.exec();
    });
    ui->moderation->setDefault(false);
    ui->moderation->setAutoDefault(false);
    connect(ui->chat, &ChatRoom::UserPinged, this, &ClientRoomWindow::ShowNotification);
    UpdateView();
}

ClientRoomWindow::~ClientRoomWindow() = default;

void ClientRoomWindow::SetModPerms(bool is_mod) {
    ui->chat->SetModPerms(is_mod);
    ui->moderation->setVisible(is_mod);
    ui->moderation->setDefault(false);
    ui->moderation->setAutoDefault(false);
}

void ClientRoomWindow::RetranslateUi() {
    ui->retranslateUi(this);
    ui->chat->RetranslateUi();
}

void ClientRoomWindow::OnRoomUpdate(const Network::RoomInformation& info) {
    UpdateView();
}

void ClientRoomWindow::OnStateChange(const Network::RoomMember::State& state) {
    if (state == Network::RoomMember::State::Joined ||
        state == Network::RoomMember::State::Moderator) {
        ui->chat->Clear();
        ui->chat->AppendStatusMessage(tr("Connected"));
        SetModPerms(state == Network::RoomMember::State::Moderator);
    }
    UpdateView();
}

void ClientRoomWindow::Disconnect() {
    auto parent = static_cast<MultiplayerState*>(parentWidget());
    if (parent->OnCloseRoom()) {
        ui->chat->AppendStatusMessage(tr("Disconnected"));
        close();
    }
}

void ClientRoomWindow::LaunchPreferredGame() {
    auto* parent = static_cast<MultiplayerState*>(parentWidget());
    if (parent == nullptr) {
        return;
    }

    const auto member = room_network.GetRoomMember().lock();
    if (!member) {
        return;
    }

    const auto room_information = member->GetRoomInformation();
    const auto program_id = room_information.preferred_game.id;
    if (program_id == 0) {
        QMessageBox::information(this, tr("No Preferred Game"),
                                 tr("This room does not advertise a preferred game."));
        return;
    }

    if (!parent->LaunchLocalGame(program_id)) {
        const auto preferred_name = QString::fromStdString(room_information.preferred_game.name);
        QMessageBox::warning(this, tr("Preferred Game Not Available"),
                             preferred_name.isEmpty()
                                 ? tr("The room's preferred game is not available in your game library.")
                                 : tr("%1 is not available in your game library.").arg(preferred_name));
    }
}

void ClientRoomWindow::UpdateView() {
    if (auto member = room_network.GetRoomMember().lock()) {
        if (member->IsConnected()) {
            ui->chat->Enable();
            ui->disconnect->setEnabled(true);
            auto memberlist = member->GetMemberInformation();
            ui->chat->SetPlayerList(memberlist);
            const auto information = member->GetRoomInformation();
            ui->launch_preferred_game->setEnabled(
                information.preferred_game.id != 0 &&
                !static_cast<MultiplayerState*>(parentWidget())
                     ->FindLocalGamePath(information.preferred_game.id)
                     .isEmpty());
            setWindowTitle(QString(tr("%1 - %2 (%3/%4 members) - connected"))
                               .arg(QString::fromStdString(information.name))
                               .arg(QString::fromStdString(information.preferred_game.name))
                               .arg(memberlist.size())
                               .arg(information.member_slots));
            ui->description->setText(QString::fromStdString(information.description));
            return;
        }
    }
    ui->launch_preferred_game->setEnabled(false);
    // TODO(B3N30): can't get RoomMember*, show error and close window
    close();
}

void ClientRoomWindow::UpdateIconDisplay() {
    ui->chat->UpdateIconDisplay();
}
