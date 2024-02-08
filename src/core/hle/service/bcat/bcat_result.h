// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/result.h"

namespace Service::BCAT {

constexpr Result ResultInvalidArgument{ErrorModule::BCAT, 1};
constexpr Result ResultFailedOpenEntity{ErrorModule::BCAT, 2};
constexpr Result ResultEntityAlreadyOpen{ErrorModule::BCAT, 6};
constexpr Result ResultNoOpenEntry{ErrorModule::BCAT, 7};

// The command to clear the delivery cache just calls fs IFileSystem DeleteFile on all of the
// files and if any of them have a non-zero result it just forwards that result. This is the FS
// error code for permission denied, which is the closest approximation of this scenario.
constexpr Result ResultFailedClearCache{ErrorModule::FS, 6400};

} // namespace Service::BCAT
