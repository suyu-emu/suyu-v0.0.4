// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/result.h"

namespace Service::Glue {

constexpr ResultCode ERR_INVALID_RESOURCE{ErrorModule::ARP, 0x1E};
constexpr ResultCode ERR_INVALID_PROCESS_ID{ErrorModule::ARP, 0x1F};
constexpr ResultCode ERR_INVALID_ACCESS{ErrorModule::ARP, 0x2A};
constexpr ResultCode ERR_NOT_REGISTERED{ErrorModule::ARP, 0x66};

} // namespace Service::Glue
