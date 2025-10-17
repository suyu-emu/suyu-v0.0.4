// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef QT_GAME_UTIL_H
#define QT_GAME_UTIL_H

#include <QObject>
#include <QStandardPaths>
#include "common/fs/path_util.h"

namespace QtCommon::Game {

enum class InstalledEntryType {
    Game,
    Update,
    AddOnContent,
};

enum class GameListRemoveTarget {
    GlShaderCache,
    VkShaderCache,
    AllShaderCache,
    CustomConfiguration,
    CacheStorage,
};

enum class ShortcutTarget {
    Desktop,
    Applications,
};

enum class ShortcutMessages{
    Fullscreen = 0,
    Success = 1,
    Volatile = 2,
    Failed = 3
};

bool CreateShortcutLink(const std::filesystem::path& shortcut_path,
                        const std::string& comment,
                        const std::filesystem::path& icon_path,
                        const std::filesystem::path& command,
                        const std::string& arguments,
                        const std::string& categories,
                        const std::string& keywords,
                        const std::string& name);

bool MakeShortcutIcoPath(const u64 program_id,
                         const std::string_view game_file_name,
                         std::filesystem::path& out_icon_path);

void OpenEdenFolder(const Common::FS::EdenPath &path);
void OpenRootDataFolder();
void OpenNANDFolder();
void OpenSaveFolder();
void OpenSDMCFolder();
void OpenModFolder();
void OpenLogFolder();

void RemoveBaseContent(u64 program_id, InstalledEntryType type);
void RemoveUpdateContent(u64 program_id, InstalledEntryType type);
void RemoveAddOnContent(u64 program_id, InstalledEntryType type);

void RemoveTransferableShaderCache(u64 program_id, GameListRemoveTarget target);
void RemoveVulkanDriverPipelineCache(u64 program_id);
void RemoveAllTransferableShaderCaches(u64 program_id);
void RemoveCustomConfiguration(u64 program_id, const std::string& game_path);
void RemoveCacheStorage(u64 program_id);

// Metadata //
void ResetMetadata(bool show_message = true);

// Shortcuts //
void CreateShortcut(const std::string& game_path,
                    const u64 program_id,
                    const std::string& game_title_,
                    const ShortcutTarget& target,
                    std::string arguments_,
                    const bool needs_title);

std::string GetShortcutPath(ShortcutTarget target);
void CreateHomeMenuShortcut(ShortcutTarget target);

}

#endif // QT_GAME_UTIL_H
