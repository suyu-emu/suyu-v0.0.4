// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/frontend/applets/profile_select.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/settings.h"

namespace Core::Frontend {

ProfileSelectApplet::~ProfileSelectApplet() = default;

void DefaultProfileSelectApplet::SelectProfile(
    std::function<void(std::optional<Common::UUID>)> callback) const {
    const auto user_index = static_cast<std::size_t>(Settings::values.current_user);
    Service::Account::ProfileManager manager;
    callback(manager.GetUser(user_index).value_or(Common::UUID{}));
    LOG_INFO(Service_ACC, "called, selecting current user instead of prompting...");
}

} // namespace Core::Frontend
