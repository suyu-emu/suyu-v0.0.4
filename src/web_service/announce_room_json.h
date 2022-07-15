// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>
#include "common/announce_multiplayer_room.h"
#include "web_service/web_backend.h"

namespace WebService {

/**
 * Implementation of AnnounceMultiplayerRoom::Backend that (de)serializes room information into/from
 * JSON, and submits/gets it to/from the yuzu web service
 */
class RoomJson : public AnnounceMultiplayerRoom::Backend {
public:
    RoomJson(const std::string& host_, const std::string& username_, const std::string& token_)
        : client(host_, username_, token_), host(host_), username(username_), token(token_) {}
    ~RoomJson() = default;
    void SetRoomInformation(const std::string& name, const std::string& description, const u16 port,
                            const u32 max_player, const u32 net_version, const bool has_password,
                            const std::string& preferred_game,
                            const u64 preferred_game_id) override;
    void AddPlayer(const AnnounceMultiplayerRoom::Member& member) override;
    WebResult Update() override;
    WebResult Register() override;
    void ClearPlayers() override;
    AnnounceMultiplayerRoom::RoomList GetRoomList() override;
    void Delete() override;

private:
    AnnounceMultiplayerRoom::Room room;
    Client client;
    std::string host;
    std::string username;
    std::string token;
    std::string room_id;
};

} // namespace WebService
