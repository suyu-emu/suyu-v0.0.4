// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include "frozen/map.h"
#include "frozen/string.h"

/// Small helper to look up enums.
/// res = the result code
/// base = the base matching value in the StringKey table
#define LOOKUP_ENUM(res, base) QtCommon::StringLookup::Lookup( \
            QtCommon::StringLookup::StringKey((int) res + (int) QtCommon::StringLookup::base))

namespace QtCommon::StringLookup {

Q_NAMESPACE

// TODO(crueter): QML interface
enum StringKey {
    DataManagerSavesTooltip,
    DataManagerShadersTooltip,
    DataManagerUserNandTooltip,
    DataManagerSysNandTooltip,
    DataManagerModsTooltip,

    // Key install results
    KeyInstallSuccess,
    KeyInstallInvalidDir,
    KeyInstallErrorFailedCopy,
    KeyInstallErrorWrongFilename,
    KeyInstallErrorFailedInit,

    // Firmware install results
    FwInstallSuccess,
    FwInstallNoNCAs,
    FwInstallFailedDelete,
    FwInstallFailedCopy,
    FwInstallFailedCorrupted,

    // Firmware Check results
    FwCheckErrorFirmwareMissing,
    FwCheckErrorFirmwareCorrupted,

    // user data migrator
    MigrationPromptPrefix,
    MigrationPrompt,
    MigrationTooltipClearShader,
    MigrationTooltipKeepOld,
    MigrationTooltipClearOld,
    MigrationTooltipLinkOld,

    // ryujinx
    KvdbNonexistent,
    KvdbNoHeader,
    KvdbInvalidMagic,
    KvdbMisaligned,
    KvdbNoImens,
    RyujinxNoSaveId,
};

// NB: the constexpr check always succeeds (in clangd at least) if size arg < size
// always triple-check the size arg
static const constexpr frozen::map<StringKey, frozen::string, 29> strings = {
    // 0-4
    {DataManagerSavesTooltip,
     QT_TR_NOOP("Contains game save data. DO NOT REMOVE UNLESS YOU KNOW WHAT YOU'RE DOING!")},
    {DataManagerShadersTooltip,
     QT_TR_NOOP("Contains Vulkan and OpenGL pipeline caches. Generally safe to remove.")},
    {DataManagerUserNandTooltip, QT_TR_NOOP("Contains updates and DLC for games.")},
    {DataManagerSysNandTooltip, QT_TR_NOOP("Contains firmware and applet data.")},
    {DataManagerModsTooltip, QT_TR_NOOP("Contains game mods, patches, and cheats.")},

    // Key install
    // 5-9
    {KeyInstallSuccess, QT_TR_NOOP("Decryption Keys were successfully installed")},
    {KeyInstallInvalidDir, QT_TR_NOOP("Unable to read key directory, aborting")},
    {KeyInstallErrorFailedCopy, QT_TR_NOOP("One or more keys failed to copy.")},
    {KeyInstallErrorWrongFilename,
     QT_TR_NOOP("Verify your keys file has a .keys extension and try again.")},
    {KeyInstallErrorFailedInit,
     QT_TR_NOOP(
         "Decryption Keys failed to initialize. Check that your dumping tools are up to date and "
         "re-dump keys.")},

    // fw install
    // 10-14
    {FwInstallSuccess, QT_TR_NOOP("Successfully installed firmware version %1")},
    {FwInstallNoNCAs, QT_TR_NOOP("Unable to locate potential firmware NCA files")},
    {FwInstallFailedDelete, QT_TR_NOOP("Failed to delete one or more firmware files.")},
    {FwInstallFailedCopy, QT_TR_NOOP("One or more firmware files failed to copy into NAND.")},
    {FwInstallFailedCorrupted,
     QT_TR_NOOP(
         "Firmware installation cancelled, firmware may be in a bad state or corrupted. Restart "
         "Eden or re-install firmware.")},

    {FwCheckErrorFirmwareMissing,
     QT_TR_NOOP(
         "Firmware missing. Firmware is required to run certain games and use the Home Menu. "
         "Versions 19.0.1 or earlier are recommended, as 20.0.0+ is currently experimental.")},
    {FwCheckErrorFirmwareCorrupted,
     QT_TR_NOOP(
         "Firmware reported as present, but was unable to be read. Check for decryption keys and "
         "redump firmware if necessary.")},

    // migrator
    // 17-22
    {MigrationPromptPrefix, QT_TR_NOOP("Eden has detected user data for the following emulators:")},
    {MigrationPrompt,
     QT_TR_NOOP("Would you like to migrate your data for use in Eden?\n"
                "Select the corresponding button to migrate data from that emulator.\n"
                "This may take a while.")},
    {MigrationTooltipClearShader,
     QT_TR_NOOP("Clearing shader cache is recommended for all "
                "users.\nDo not uncheck unless you know what "
                "you're doing.")},
    {MigrationTooltipKeepOld,
     QT_TR_NOOP("Keeps the old data directory. This is recommended if you aren't\n"
                "space-constrained and want to keep separate data for the old emulator.")},
    {MigrationTooltipClearOld,
     QT_TR_NOOP("Deletes the old data directory.\nThis is recommended on "
                "devices with space constraints.")},
    {MigrationTooltipLinkOld,
     QT_TR_NOOP("Creates a filesystem link between the old directory and Eden directory.\n"
                "This is recommended if you want to share data between emulators.")},

    // why am I writing these comments again
    // 23-28
    {KvdbNonexistent, QT_TR_NOOP("Ryujinx title database does not exist.")},
    {KvdbNoHeader, QT_TR_NOOP("Invalid header on Ryujinx title database.")},
    {KvdbInvalidMagic, QT_TR_NOOP("Invalid magic header on Ryujinx title database.")},
    {KvdbMisaligned, QT_TR_NOOP("Invalid byte alignment on Ryujinx title database.")},
    {KvdbNoImens, QT_TR_NOOP("No items found in Ryujinx title database.")},
    {RyujinxNoSaveId, QT_TR_NOOP("Title %1 not found in Ryujinx title database.")},
};

static inline const QString Lookup(StringKey key)
{
    return QObject::tr(strings.at(key).data());
}

} // namespace QtCommon::StringLookup
