// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/result.h"

namespace Service::PSC::Time {

constexpr Result ResultPermissionDenied{ErrorModule::TimeService, 1};
constexpr Result ResultClockMismatch{ErrorModule::TimeService, 102};
constexpr Result ResultClockUninitialized{ErrorModule::TimeService, 103};
constexpr Result ResultTimeNotFound{ErrorModule::TimeService, 200};
constexpr Result ResultOverflow{ErrorModule::TimeService, 201};
constexpr Result ResultFailed{ErrorModule::TimeService, 801};
constexpr Result ResultInvalidArgument{ErrorModule::TimeService, 901};
constexpr Result ResultTimeZoneOutOfRange{ErrorModule::TimeService, 902};
constexpr Result ResultTimeZoneParseFailed{ErrorModule::TimeService, 903};
constexpr Result ResultRtcTimeout{ErrorModule::TimeService, 988};
constexpr Result ResultTimeZoneNotFound{ErrorModule::TimeService, 989};
constexpr Result ResultNotImplemented{ErrorModule::TimeService, 990};
constexpr Result ResultAlarmNotRegistered{ErrorModule::TimeService, 1502};

} // namespace Service::PSC::Time
