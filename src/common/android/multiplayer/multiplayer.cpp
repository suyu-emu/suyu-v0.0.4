// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/android/id_cache.h"
#include "multiplayer.h"

#include "common/android/android_common.h"

#include "core/core.h"
#include "network/network.h"
#include "android/log.h"

#include "common/settings.h"
#include "web_service/web_backend.h"
#include "web_service/verify_user_jwt.h"
#include "web_service/web_result.h"

#include <thread>
#include <chrono>

namespace IDCache = Common::Android;

AndroidMultiplayer::AndroidMultiplayer(Core::System &system_,
                                       std::shared_ptr<Core::AnnounceMultiplayerSession> session)
        : system{system_}, announce_multiplayer_session(session) {}

AndroidMultiplayer::~AndroidMultiplayer() = default;

void AndroidMultiplayer::AddNetPlayMessage(jint type, jstring msg) {
    IDCache::GetEnvForThread()->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(),
                                                     IDCache::GetAddNetPlayMessage(), type, msg);
}

void AndroidMultiplayer::AddNetPlayMessage(int type, const std::string &msg) {
    JNIEnv *env = IDCache::GetEnvForThread();
    AddNetPlayMessage(type, Common::Android::ToJString(env, msg));
}

void AndroidMultiplayer::ClearChat() {
    IDCache::GetEnvForThread()->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(),
                                                     IDCache::ClearChat());
}

bool AndroidMultiplayer::NetworkInit() {
    bool result = Network::Init();

    if (!result) {
        return false;
    }

    if (auto member = Network::GetRoomMember().lock()) {
        // register the network structs to use in slots and signals
        member->BindOnStateChanged([this](const Network::RoomMember::State &state) {
            if (state == Network::RoomMember::State::Joined ||
                state == Network::RoomMember::State::Moderator) {
                NetPlayStatus status;
                std::string msg;
                switch (state) {
                    case Network::RoomMember::State::Joined:
                        status = NetPlayStatus::ROOM_JOINED;
                        break;
                    case Network::RoomMember::State::Moderator:
                        status = NetPlayStatus::ROOM_MODERATOR;
                        break;
                    default:
                        return;
                }
                AddNetPlayMessage(static_cast<int>(status), msg);
            }
        });
        member->BindOnError([this](const Network::RoomMember::Error &error) {
            NetPlayStatus status;
            std::string msg;
            switch (error) {
                case Network::RoomMember::Error::LostConnection:
                    status = NetPlayStatus::LOST_CONNECTION;
                    break;
                case Network::RoomMember::Error::HostKicked:
                    status = NetPlayStatus::HOST_KICKED;
                    break;
                case Network::RoomMember::Error::UnknownError:
                    status = NetPlayStatus::UNKNOWN_ERROR;
                    break;
                case Network::RoomMember::Error::NameCollision:
                    status = NetPlayStatus::NAME_COLLISION;
                    break;
                case Network::RoomMember::Error::IpCollision:
                    status = NetPlayStatus::MAC_COLLISION;
                    break;
                case Network::RoomMember::Error::WrongVersion:
                    status = NetPlayStatus::WRONG_VERSION;
                    break;
                case Network::RoomMember::Error::WrongPassword:
                    status = NetPlayStatus::WRONG_PASSWORD;
                    break;
                case Network::RoomMember::Error::CouldNotConnect:
                    status = NetPlayStatus::COULD_NOT_CONNECT;
                    break;
                case Network::RoomMember::Error::RoomIsFull:
                    status = NetPlayStatus::ROOM_IS_FULL;
                    break;
                case Network::RoomMember::Error::HostBanned:
                    status = NetPlayStatus::HOST_BANNED;
                    break;
                case Network::RoomMember::Error::PermissionDenied:
                    status = NetPlayStatus::PERMISSION_DENIED;
                    break;
                case Network::RoomMember::Error::NoSuchUser:
                    status = NetPlayStatus::NO_SUCH_USER;
                    break;
            }
            AddNetPlayMessage(static_cast<int>(status), msg);
        });
        member->BindOnStatusMessageReceived(
                [this](const Network::StatusMessageEntry &status_message) {
                    NetPlayStatus status = NetPlayStatus::NO_ERROR;
                    std::string msg(status_message.nickname);
                    switch (status_message.type) {
                        case Network::IdMemberJoin:
                            status = NetPlayStatus::MEMBER_JOIN;
                            break;
                        case Network::IdMemberLeave:
                            status = NetPlayStatus::MEMBER_LEAVE;
                            break;
                        case Network::IdMemberKicked:
                            status = NetPlayStatus::MEMBER_KICKED;
                            break;
                        case Network::IdMemberBanned:
                            status = NetPlayStatus::MEMBER_BANNED;
                            break;
                        case Network::IdAddressUnbanned:
                            status = NetPlayStatus::ADDRESS_UNBANNED;
                            break;
                    }
                    AddNetPlayMessage(static_cast<int>(status), msg);
                });
        member->BindOnChatMessageReceived([this](const Network::ChatEntry &chat) {
            NetPlayStatus status = NetPlayStatus::CHAT_MESSAGE;
            std::string msg(chat.nickname);
            msg += ": ";
            msg += chat.message;
            AddNetPlayMessage(static_cast<int>(status), msg);
        });
    }

    return true;
}

NetPlayStatus AndroidMultiplayer::NetPlayCreateRoom(const std::string &ipaddress, int port,
                                                    const std::string &username,
                                                    const std::string &preferredGameName,
                                                    const u64 &preferredGameId,
                                                    const std::string &password,
                                                    const std::string &room_name, int max_players,
                                                    bool isPublic) {
    auto member = Network::GetRoomMember().lock();
    if (!member) {
        return NetPlayStatus::NETWORK_ERROR;
    }

    if (member->GetState() == Network::RoomMember::State::Joining || member->IsConnected()) {
        return NetPlayStatus::ALREADY_IN_ROOM;
    }

    auto room = Network::GetRoom().lock();
    if (!room) {
        return NetPlayStatus::NETWORK_ERROR;
    }

    if (room_name.length() < 3 || room_name.length() > 20) {
        return NetPlayStatus::CREATE_ROOM_ERROR;
    }

    // Placeholder game info
    const AnnounceMultiplayerRoom::GameInfo game{
            .name = preferredGameName,
            .id = preferredGameId,
    };

    port = (port == 0) ? Network::DefaultRoomPort : static_cast<u16>(port);

    if (!room->Create(room_name, "", ipaddress, static_cast<u16>(port), password,
                      static_cast<u32>(std::min(max_players, 16)), username, game,
                      CreateVerifyBackend(isPublic), {})) {
        return NetPlayStatus::CREATE_ROOM_ERROR;
    }

    // public announce session
    if (isPublic) {
        if (auto session = announce_multiplayer_session.lock()) {
            WebService::WebResult result = session->Register();
            if (result.result_code != WebService::WebResult::Code::Success) {
                LOG_ERROR(WebService, "Failed to announce public room lobby");
                room->Destroy();
                return NetPlayStatus::CREATE_ROOM_ERROR;
            }
            session->Start();
        } else {
            LOG_ERROR(Network, "Failed to start announce session");
        }
    }

    // Failsafe timer to avoid joining before creation
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string token;
    // TODO(alekpop): properly handle the compile definition, it's not working right
#ifdef ENABLE_WEB_SERVICE
    LOG_INFO(WebService, "Web Service enabled");
    if (isPublic) {
        WebService::Client client(Settings::values.web_api_url.GetValue(),
                                  Settings::values.eden_username.GetValue(),
                                  Settings::values.eden_token.GetValue());

        token = client.GetExternalJWT(room->GetVerifyUID()).returned_data;

        if (token.empty()) {
            LOG_ERROR(WebService, "Could not get external JWT, verification may fail");
        } else {
            LOG_INFO(WebService, "Successfully requested external JWT: size={}", token.size());
        }
    }
#else
    LOG_ERROR(WebService, "Web Service disabled");
#endif

    member->Join(username, ipaddress.c_str(), static_cast<u16>(port), 0, Network::NoPreferredIP,
                 password, token);

    // Failsafe timer to avoid joining before creation
    for (int i = 0; i < 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (member->GetState() == Network::RoomMember::State::Joined ||
            member->GetState() == Network::RoomMember::State::Moderator) {
            return NetPlayStatus::NO_ERROR;
        }
    }

    // If join failed while room is created, clean up the room
    room->Destroy();
    return NetPlayStatus::CREATE_ROOM_ERROR;
}

NetPlayStatus AndroidMultiplayer::NetPlayJoinRoom(const std::string &ipaddress, int port,
                                                  const std::string &username,
                                                  const std::string &password) {
    auto member = Network::GetRoomMember().lock();
    if (!member) {
        return NetPlayStatus::NETWORK_ERROR;
    }

    port =
            (port == 0) ? Network::DefaultRoomPort : static_cast<u16>(port);


    if (member->GetState() == Network::RoomMember::State::Joining || member->IsConnected()) {
        return NetPlayStatus::ALREADY_IN_ROOM;
    }

    member->Join(username, ipaddress.c_str(), static_cast<u16>(port), 0, Network::NoPreferredIP,
                 password, "");

    // Wait a bit for the connection and join process to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (member->GetState() == Network::RoomMember::State::Joined ||
        member->GetState() == Network::RoomMember::State::Moderator) {
        return NetPlayStatus::NO_ERROR;
    }

    if (!member->IsConnected()) {
        return NetPlayStatus::COULD_NOT_CONNECT;
    }

    return NetPlayStatus::WRONG_PASSWORD;
}

void AndroidMultiplayer::NetPlaySendMessage(const std::string &msg) {
    if (auto room = Network::GetRoomMember().lock()) {
        if (room->GetState() != Network::RoomMember::State::Joined &&
            room->GetState() != Network::RoomMember::State::Moderator) {

            return;
        }
        room->SendChatMessage(msg);
    }
}

void AndroidMultiplayer::NetPlayKickUser(const std::string &username) {
    if (auto room = Network::GetRoomMember().lock()) {
        auto members = room->GetMemberInformation();
        auto it = std::find_if(members.begin(), members.end(),
                               [&username](const Network::RoomMember::MemberInformation &member) {
                                   return member.nickname == username;
                               });
        if (it != members.end()) {
            room->SendModerationRequest(Network::RoomMessageTypes::IdModKick, username);
        }
    }
}

void AndroidMultiplayer::NetPlayBanUser(const std::string &username) {
    if (auto room = Network::GetRoomMember().lock()) {
        auto members = room->GetMemberInformation();
        auto it = std::find_if(members.begin(), members.end(),
                               [&username](const Network::RoomMember::MemberInformation &member) {
                                   return member.nickname == username;
                               });
        if (it != members.end()) {
            room->SendModerationRequest(Network::RoomMessageTypes::IdModBan, username);
        }
    }
}

void AndroidMultiplayer::NetPlayUnbanUser(const std::string &username) {
    if (auto room = Network::GetRoomMember().lock()) {
        room->SendModerationRequest(Network::RoomMessageTypes::IdModUnban, username);
    }
}

std::vector<std::string> AndroidMultiplayer::NetPlayRoomInfo() {
    std::vector<std::string> info_list;
    if (auto room = Network::GetRoomMember().lock()) {
        auto members = room->GetMemberInformation();
        if (!members.empty()) {
            // name and max players
            auto room_info = room->GetRoomInformation();
            info_list.push_back(room_info.name + "|" + std::to_string(room_info.member_slots));
            // all members
            for (const auto &member: members) {
                info_list.push_back(member.nickname);
            }
        }
    }
    return info_list;
}

bool AndroidMultiplayer::NetPlayIsJoined() {
    auto member = Network::GetRoomMember().lock();
    if (!member) {
        return false;
    }

    return (member->GetState() == Network::RoomMember::State::Joined ||
            member->GetState() == Network::RoomMember::State::Moderator);
}

bool AndroidMultiplayer::NetPlayIsHostedRoom() {
    if (auto room = Network::GetRoom().lock()) {
        return room->GetState() == Network::Room::State::Open;
    }
    return false;
}

void AndroidMultiplayer::NetPlayLeaveRoom() {
    if (auto room = Network::GetRoom().lock()) {
        // if you are in a room, leave it
        if (auto member = Network::GetRoomMember().lock()) {
            member->Leave();
        }

        ClearChat();

        // if you are hosting a room, also stop hosting
        if (room->GetState() == Network::Room::State::Open) {
            room->Destroy();
        }
    }
}

void AndroidMultiplayer::NetworkShutdown() {
    Network::Shutdown();
}

bool AndroidMultiplayer::NetPlayIsModerator() {
    auto member = Network::GetRoomMember().lock();
    if (!member) {
        return false;
    }
    return member->GetState() == Network::RoomMember::State::Moderator;
}

std::vector<std::string> AndroidMultiplayer::NetPlayGetPublicRooms() {
    std::vector<std::string> room_list;

    if (auto session = announce_multiplayer_session.lock()) {
        auto rooms = session->GetRoomList();
        for (const auto &room: rooms) {
            room_list.push_back(room.information.name + "|" +
                                (room.has_password ? "1" : "0") + "|" +
                                std::to_string(room.information.member_slots) + "|" +
                                room.ip + "|" +
                                std::to_string(room.information.port) + "|" +
                                room.information.description + "|" +
                                room.information.host_username + "|" +
                                std::to_string(room.information.preferred_game.id) + "|" +
                                room.information.preferred_game.name + "|" +
                                room.information.preferred_game.version);

            for (const auto &member: room.members) {
                room_list.push_back("MEMBER|" + room.information.name + "|" +
                                    member.username + "|" +
                                    member.nickname + "|" +
                                    std::to_string(member.game.id) + "|" +
                                    member.game.name);
            }
        }

    }
    return room_list;

}

std::vector<std::string> AndroidMultiplayer::NetPlayGetBanList() {
    std::vector<std::string> ban_list;
    if (auto room = Network::GetRoom().lock()) {
        auto [username_bans, ip_bans] = room->GetBanList();

        // Add username bans
        for (const auto &username: username_bans) {
            ban_list.push_back(username);
        }

        // Add IP bans
        for (const auto &ip: ip_bans) {
            ban_list.push_back(ip);
        }
    }
    return ban_list;
}

std::unique_ptr<Network::VerifyUser::Backend> AndroidMultiplayer::CreateVerifyBackend(bool use_validation) {
    std::unique_ptr<Network::VerifyUser::Backend> verify_backend;
    if (use_validation) {
#ifdef ENABLE_WEB_SERVICE
        verify_backend =
            std::make_unique<WebService::VerifyUserJWT>(Settings::values.web_api_url.GetValue());
#else
        verify_backend = std::make_unique<Network::VerifyUser::NullBackend>();
#endif
    } else {
        verify_backend = std::make_unique<Network::VerifyUser::NullBackend>();
    }
    return verify_backend;
}