// Copyright 2022 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>

#include "core/hle/service/mii/types.h"

namespace Core::Frontend {

struct MiiParameters {
    bool is_editable;
    Service::Mii::MiiInfo mii_data{};
};

class MiiEditApplet {
public:
    virtual ~MiiEditApplet();

    virtual void ShowMii(const MiiParameters& parameters,
                         const std::function<void(const Core::Frontend::MiiParameters& parameters)>
                             callback) const = 0;
};

class DefaultMiiEditApplet final : public MiiEditApplet {
public:
    void ShowMii(const MiiParameters& parameters,
                 const std::function<void(const Core::Frontend::MiiParameters& parameters)>
                     callback) const override;
};

} // namespace Core::Frontend
