// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/result.h"

namespace Service::NFC {

constexpr Result ResultDeviceNotFound(ErrorModule::NFC, 64);
constexpr Result ResultInvalidArgument(ErrorModule::NFC, 65);
constexpr Result ResultWrongApplicationAreaSize(ErrorModule::NFC, 68);
constexpr Result ResultWrongDeviceState(ErrorModule::NFC, 73);
constexpr Result ResultUnknown74(ErrorModule::NFC, 74);
constexpr Result ResultUnknown76(ErrorModule::NFC, 76);
constexpr Result ResultNfcNotInitialized(ErrorModule::NFC, 77);
constexpr Result ResultNfcDisabled(ErrorModule::NFC, 80);
constexpr Result ResultWriteAmiiboFailed(ErrorModule::NFC, 88);
constexpr Result ResultTagRemoved(ErrorModule::NFC, 97);
constexpr Result ResultUnableToAccessBackupFile(ErrorModule::NFC, 113);
constexpr Result ResultRegistrationIsNotInitialized(ErrorModule::NFC, 120);
constexpr Result ResultApplicationAreaIsNotInitialized(ErrorModule::NFC, 128);
constexpr Result ResultCorruptedDataWithBackup(ErrorModule::NFC, 136);
constexpr Result ResultCorruptedData(ErrorModule::NFC, 144);
constexpr Result ResultWrongApplicationAreaId(ErrorModule::NFC, 152);
constexpr Result ResultApplicationAreaExist(ErrorModule::NFC, 168);
constexpr Result ResultNotAnAmiibo(ErrorModule::NFC, 178);
constexpr Result ResultBackupPathAlreadyExist(ErrorModule::NFC, 216);

} // namespace Service::NFC
