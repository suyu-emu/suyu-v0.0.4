// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <optional>
#include "common/new_uuid.h"

namespace Core::Frontend {

class ProfileSelectApplet {
public:
    virtual ~ProfileSelectApplet();

    virtual void SelectProfile(
        std::function<void(std::optional<Common::NewUUID>)> callback) const = 0;
};

class DefaultProfileSelectApplet final : public ProfileSelectApplet {
public:
    void SelectProfile(std::function<void(std::optional<Common::NewUUID>)> callback) const override;
};

} // namespace Core::Frontend
