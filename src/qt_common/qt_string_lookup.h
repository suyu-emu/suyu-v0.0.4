// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <frozen/string.h>
#include <frozen/unordered_map.h>
#include <qobjectdefs.h>
#include <qtmetamacros.h>

namespace QtCommon::StringLookup {

Q_NAMESPACE

// TODO(crueter): QML interface
enum StringKey {
    SavesTooltip,
    ShadersTooltip,
    UserNandTooltip,
    SysNandTooltip,
    ModsTooltip,
};

static constexpr const frozen::unordered_map<StringKey, frozen::string, 5> strings = {
    {SavesTooltip, "Contains game save data. DO NOT REMOVE UNLESS YOU KNOW WHAT YOU'RE DOING!"},
    {ShadersTooltip, "Contains Vulkan and OpenGL pipeline caches. Generally safe to remove."},
    {UserNandTooltip, "Contains updates and DLC for games."},
    {SysNandTooltip, "Contains firmware and applet data."},
    {ModsTooltip, "Contains game mods, patches, and cheats."},
};

static inline const QString Lookup(StringKey key)
{
    return QString::fromStdString(strings.at(key).data());
}

}
