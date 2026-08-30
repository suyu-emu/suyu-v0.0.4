// SPDX-FileCopyrightText: Copyright 2025 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QSettings>
#include <QString>

namespace GraphicsBackend {

QString GuiConfigPath();
void SetForceX11(bool state);
bool GetForceX11();

} // namespace GraphicsBackend
