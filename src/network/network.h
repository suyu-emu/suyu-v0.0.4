// SPDX-FileCopyrightText: Copyright 2017 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

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

} // namespace Network
