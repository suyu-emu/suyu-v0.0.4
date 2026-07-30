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
    const QString room_name = QString::fromStdString(info.name);
    if (!room_name.isEmpty() && room_name != last_room_name_) {
        last_room_name_ = room_name;
        last_announced_program_id_ = 0;
        auto_launch_attempted_ = false;
        ui->chat->AppendStatusMessage(tr("Joined room %1. Waiting for room details...").arg(room_name));
    }

    if (info.preferred_game.id != last_announced_program_id_) {
        last_announced_program_id_ = info.preferred_game.id;
        auto_launch_attempted_ = false;

        if (info.preferred_game.id == 0) {
            ui->chat->AppendStatusMessage(
                tr("Connected to the room chat. Waiting for the host to advertise a multiplayer game."));
        } else if (auto* parent = static_cast<MultiplayerState*>(parentWidget())) {
            const auto preferred_name = QString::fromStdString(info.preferred_game.name);
            const auto game_name =
                preferred_name.isEmpty() ? tr("the preferred game") : preferred_name;
            const auto local_path = parent->FindLocalGamePath(info.preferred_game.id);
            if (local_path.isEmpty()) {
                ui->chat->AppendStatusMessage(
                    tr("Room prefers %1, but no matching local ROM was found in your library.").arg(
                        game_name));
            } else {
                ui->chat->AppendStatusMessage(
                    tr("Room prefers %1. Launching your local copy automatically.").arg(game_name));
            }
        }
    }

    UpdateView();
    MaybeAutoLaunchPreferredGame(info);
}

void ClientRoomWindow::OnStateChange(const Network::RoomMember::State& state) {
    if (state == Network::RoomMember::State::Joined ||
        state == Network::RoomMember::State::Moderator) {
        ui->chat->Clear();
        ui->chat->AppendStatusMessage(tr("Connected"));
        ui->chat->AppendStatusMessage(
            tr("This room bridges the game's Local Play / Local Wireless mode only. "
               "In-game, open Local Play (not Online/Elite Smash) to find this session — "
               "internet-based online modes require real Nintendo servers and cannot be "
               "routed through rooms."));
        SetModPerms(state == Network::RoomMember::State::Moderator);
        if (const auto member = room_network.GetRoomMember().lock()) {
            MaybeAutoLaunchPreferredGame(member->GetRoomInformation());
        }
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
    const auto preferred_name = QString::fromStdString(room_information.preferred_game.name);
    if (program_id == 0 && preferred_name.trimmed().isEmpty()) {
        QMessageBox::information(this, tr("No Preferred Game"),
                                 tr("This room does not advertise a preferred game."));
        return;
    }

    // By ID first, then by the advertised name.
    QString local_path = parent->FindLocalGamePath(program_id);
    if (local_path.isEmpty()) {
        local_path = parent->FindLocalGameByName(preferred_name);
    }

    if (!parent->LaunchLocalGamePath(local_path)) {
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
            // Enabled when the game can be found either way - by title ID, or
            // by the name the room advertises. Requiring an ID match left the
            // button dead for rooms whose ID does not line up with the local
            // copy, which is most of them.
            auto* state = static_cast<MultiplayerState*>(parentWidget());
            const bool have_by_id =
                information.preferred_game.id != 0 &&
                !state->FindLocalGamePath(information.preferred_game.id).isEmpty();
            const bool have_by_name =
                !state->FindLocalGameByName(
                          QString::fromStdString(information.preferred_game.name))
                     .isEmpty();
            ui->launch_preferred_game->setEnabled(have_by_id || have_by_name);
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

void ClientRoomWindow::MaybeAutoLaunchPreferredGame(const Network::RoomInformation& info) {
    if (auto_launch_attempted_ || info.preferred_game.id == 0) {
        return;
    }

    auto* parent = static_cast<MultiplayerState*>(parentWidget());
    if (parent == nullptr) {
        return;
    }

    const auto preferred_name = QString::fromStdString(info.preferred_game.name);
    auto local_path = parent->FindLocalGamePath(info.preferred_game.id);
    if (local_path.isEmpty()) {
        local_path = parent->FindLocalGameByName(preferred_name);
    }
    if (local_path.isEmpty()) {
        return;
    }

    auto_launch_attempted_ = true;
    const auto game_name = preferred_name.isEmpty() ? tr("the preferred game") : preferred_name;

    if (parent->LaunchLocalGamePath(local_path)) {
        ui->chat->AppendStatusMessage(
            tr("Launched your local copy of %1 to match the room.").arg(game_name));
    } else {
        auto_launch_attempted_ = false;
        ui->chat->AppendStatusMessage(
            tr("Automatic launch for %1 failed. Use Launch Preferred Game to retry.").arg(
                game_name));
    }
}

void ClientRoomWindow::UpdateIconDisplay() {
    ui->chat->UpdateIconDisplay();
}
