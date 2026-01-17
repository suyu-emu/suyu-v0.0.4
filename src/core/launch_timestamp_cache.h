// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"

namespace Core::LaunchTimestampCache {

void SaveLaunchTimestamp(u64 title_id);
s64 GetLaunchTimestamp(u64 title_id);

} // namespace Core::LaunchTimestampCache
