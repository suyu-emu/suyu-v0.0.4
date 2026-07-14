// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright 2017 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include "network/room.h"
#include "network/room_member.h"

namespace Network {

/// Initializes and registers the network device, the room, and the room member.
bool Init();

/// Returns a pointer to the room handle
std::weak_ptr<Room> GetRoom();

/// Returns a pointer to the room member handle
std::weak_ptr<RoomMember> GetRoomMember();

/// Unregisters the network device, the room, and the room member and shut them down.
void Shutdown();

/// suyu compatibility wrapper — suyu's frontend passes RoomNetwork& to multiplayer dialogs
struct RoomNetwork {
    bool Init()     { return Network::Init(); }
    void Shutdown() { return Network::Shutdown(); }
    std::weak_ptr<Room>       GetRoom()       const { return Network::GetRoom(); }
    std::weak_ptr<RoomMember> GetRoomMember() const { return Network::GetRoomMember(); }
};

} // namespace Network
