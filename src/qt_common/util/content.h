// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef QT_CONTENT_UTIL_H
#define QT_CONTENT_UTIL_H

#include <QObject>
#include "common/common_types.h"
#include "frontend_common/data_manager.h"
#include "frontend_common/firmware_manager.h"
#include "qt_common/qt_string_lookup.h"

namespace QtCommon::Content {

//
bool CheckGameFirmware(u64 program_id, QObject *parent);

enum class FirmwareInstallResult {
    Success,
    NoNCAs,
    FailedDelete,
    FailedCopy,
    FailedCorrupted,
};

inline const QString GetFirmwareInstallResultString(FirmwareInstallResult result)
{
    return LOOKUP_ENUM(result, FwInstallSuccess);
}

/**
 * \brief Get a string representation of a result from InstallKeys.
 * \param result The result code.
 * \return A string representation of the passed result code.
 */
inline const QString GetKeyInstallResultString(FirmwareManager::KeyInstallResult result)
{
    return LOOKUP_ENUM(result, KeyInstallSuccess);
}

void InstallFirmware(const QString &location, bool recursive);

QString UnzipFirmwareToTmp(const QString &location);

// Keys //
void InstallKeys();

// Content //
void VerifyGameContents(const std::string &game_path);
void VerifyInstalledContents();

void ClearDataDir(FrontendCommon::DataManager::DataDir dir, const std::string &user_id = "");
void ExportDataDir(FrontendCommon::DataManager::DataDir dir,
                   const std::string &user_id = "",
                   const QString &name = QStringLiteral("export"),
                   std::function<void()> callback = {});
void ImportDataDir(FrontendCommon::DataManager::DataDir dir, const std::string &user_id = "", std::function<void()> callback = {});

// Profiles //
void FixProfiles();
}
#endif // QT_CONTENT_UTIL_H
