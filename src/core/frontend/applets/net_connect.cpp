// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/logging.h"
#include "core/frontend/applets/net_connect.h"

namespace Core::Frontend {

NetConnectApplet::~NetConnectApplet() = default;

void DefaultNetConnectApplet::Close() const {}

} // namespace Core::Frontend
