// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/result.h"

namespace Service::Mii {

constexpr Result ResultInvalidArgument{ErrorModule::Mii, 1};
constexpr Result ResultInvalidArgumentSize{ErrorModule::Mii, 2};
constexpr Result ResultNotUpdated{ErrorModule::Mii, 3};
constexpr Result ResultNotFound{ErrorModule::Mii, 4};
constexpr Result ResultDatabaseFull{ErrorModule::Mii, 5};
constexpr Result ResultInvalidCharInfo{ErrorModule::Mii, 100};
constexpr Result ResultInvalidStoreData{ErrorModule::Mii, 109};
constexpr Result ResultInvalidOperation{ErrorModule::Mii, 202};
constexpr Result ResultPermissionDenied{ErrorModule::Mii, 203};

}; // namespace Service::Mii
