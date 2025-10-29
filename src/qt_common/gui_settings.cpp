// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gui_settings.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"

namespace FS = Common::FS;

namespace GraphicsBackend {

QString GuiConfigPath() {
    return QString::fromStdString(FS::PathToUTF8String(FS::GetEdenPath(FS::EdenPath::ConfigDir) / "gui_config.ini"));
}

void SetForceX11(bool state) {
    (void)FS::CreateParentDir(GuiConfigPath().toStdString());
    QSettings(GuiConfigPath(), QSettings::IniFormat).setValue("gui_force_x11", state);
}

bool GetForceX11() {
    return QSettings(GuiConfigPath(), QSettings::IniFormat).value("gui_force_x11", false).toBool();
}

} // namespace GraphicsBackend
