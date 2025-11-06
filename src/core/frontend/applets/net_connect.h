// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>

#include "core/frontend/applets/applet.h"

namespace Core::Frontend {

class NetConnectApplet : public Applet {
public:
    virtual ~NetConnectApplet();
};

class DefaultNetConnectApplet final : public NetConnectApplet {
public:
    void Close() const override;
};

} // namespace Core::Frontend
