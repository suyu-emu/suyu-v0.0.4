// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FIRMWARE_MANAGER_H
#define FIRMWARE_MANAGER_H

#include "common/common_types.h"
#include "core/core.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/registered_cache.h"
#include "core/hle/service/filesystem/filesystem.h"
#include <algorithm>
#include <array>
#include <core/hle/service/am/frontend/applet_mii_edit.h>
#include <string>

#include "core/hle/result.h"
#include "core/hle/service/set/settings_types.h"
#include "core/hle/service/set/system_settings_server.h"

namespace FirmwareManager {

static constexpr std::array<u64, 1> FIRMWARE_REQUIRED_GAMES = {
    0x0100152000022000ULL, // MK8DX
};

enum KeyInstallResult {
    Success,
    InvalidDir,
    ErrorFailedCopy,
    ErrorWrongFilename,
    ErrorFailedInit,
};

/**
 * @brief Installs any arbitrary set of keys for the emulator.
 * @param location Where the keys are located.
 * @param expected_extension What extension the file should have.
 * @return A result code for the operation.
 */
KeyInstallResult InstallKeys(std::string location, std::string expected_extension);

/**
 * \brief Check if the specified program requires firmware to run properly.
 * It is the responsibility of the frontend to properly expose this to the user.
 * \param program_id The program ID to check.
 * \return Whether or not the program requires firmware to run properly.
 */
inline constexpr bool GameRequiresFirmware(u64 program_id)
{
    return std::find(FIRMWARE_REQUIRED_GAMES.begin(), FIRMWARE_REQUIRED_GAMES.end(), program_id)
           != FIRMWARE_REQUIRED_GAMES.end();
}

enum FirmwareCheckResult {
    FirmwareGood,
    ErrorFirmwareMissing,
    ErrorFirmwareCorrupted,
};

/**
 * \brief Checks for installed firmware within the system.
 * \param system The system to check for firmware.
 * \return Whether or not the system has installed firmware.
 */
inline bool CheckFirmwarePresence(Core::System &system)
{
    constexpr u64 MiiEditId = static_cast<u64>(Service::AM::AppletProgramId::MiiEdit);

    auto bis_system = system.GetFileSystemController().GetSystemNANDContents();
    if (!bis_system) {
        return false;
    }

    auto mii_applet_nca = bis_system->GetEntry(MiiEditId, FileSys::ContentRecordType::Program);

    if (!mii_applet_nca) {
        return false;
    }

    return true;
}

/**
 * \brief Verifies if firmware is properly installed and is in the correct version range.
 * \param system The system to check firmware on.
 * \return A result code defining the status of the system's firmware.
 */
FirmwareCheckResult VerifyFirmware(Core::System &system);

/**
 * @brief Get the currently installed firmware version.
 * @param system The system to check firmware on.
 * @return A pair of the firmware version format and result code.
 */
inline std::pair<Service::Set::FirmwareVersionFormat, Result> GetFirmwareVersion(Core::System &system)
{
    Service::Set::FirmwareVersionFormat firmware_data{};
    const auto result
        = Service::Set::GetFirmwareVersionImpl(firmware_data,
                                               system,
                                               Service::Set::GetFirmwareVersionType::Version2);

    return {firmware_data, result};
}

// TODO(crueter): GET AS STRING
}

#endif
