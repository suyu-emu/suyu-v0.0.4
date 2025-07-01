// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>

#include <common/common_types.h>
#include <network/network.h>
#include <network/announce_multiplayer_session.h>

namespace Core {
    class System;
    class AnnounceMultiplayerSession;
}

enum class NetPlayStatus : s32 {
    NO_ERROR,

    NETWORK_ERROR,
    LOST_CONNECTION,
    NAME_COLLISION,
    MAC_COLLISION,
    CONSOLE_ID_COLLISION,
    WRONG_VERSION,
    WRONG_PASSWORD,
    COULD_NOT_CONNECT,
    ROOM_IS_FULL,
    HOST_BANNED,
    PERMISSION_DENIED,
    NO_SUCH_USER,
    ALREADY_IN_ROOM,
    CREATE_ROOM_ERROR,
    HOST_KICKED,
    UNKNOWN_ERROR,

    ROOM_UNINITIALIZED,
    ROOM_IDLE,
    ROOM_JOINING,
    ROOM_JOINED,
    ROOM_MODERATOR,

    MEMBER_JOIN,
    MEMBER_LEAVE,
    MEMBER_KICKED,
    MEMBER_BANNED,
    ADDRESS_UNBANNED,

    CHAT_MESSAGE,
};

class AndroidMultiplayer {
public:
    explicit AndroidMultiplayer(Core::System& system,
                                std::shared_ptr<Core::AnnounceMultiplayerSession> session);
    ~AndroidMultiplayer();

    bool NetworkInit();

    void AddNetPlayMessage(int status, const std::string& msg);
    void AddNetPlayMessage(jint type, jstring msg);

    void ClearChat();

    NetPlayStatus NetPlayCreateRoom(const std::string &ipaddress, int port,
                                    const std::string &username, const std::string &preferredGameName,
                                    const u64 &preferredGameId, const std::string &password,
                                    const std::string &room_name, int max_players, bool isPublic);

    NetPlayStatus NetPlayJoinRoom(const std::string &ipaddress, int port,
                                  const std::string &username, const std::string &password);

    std::vector<std::string> NetPlayRoomInfo();

    bool NetPlayIsJoined();

    bool NetPlayIsHostedRoom();

    bool NetPlayIsModerator();

    void NetPlaySendMessage(const std::string &msg);

    void NetPlayKickUser(const std::string &username);

    void NetPlayBanUser(const std::string &username);

    void NetPlayLeaveRoom();

    static void NetworkShutdown();

    std::vector<std::string> NetPlayGetBanList();

    void NetPlayUnbanUser(const std::string &username);

    std::vector<std::string> NetPlayGetPublicRooms();

private:
    Core::System& system;
    static std::unique_ptr<Network::VerifyUser::Backend> CreateVerifyBackend(bool use_validation) ;
    std::weak_ptr<Core::AnnounceMultiplayerSession> announce_multiplayer_session;
};
