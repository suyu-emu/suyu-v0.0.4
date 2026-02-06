// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include "common/common_types.h"
#include "frontend_common/mod_manager.h"

namespace QtCommon::Mod {

QStringList GetModFolders(const QString &root, const QString &fallbackName);

const QString ExtractMod(const QString &path);

}
