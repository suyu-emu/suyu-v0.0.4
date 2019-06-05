// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "input_common/main.h"

namespace InputCommon::Polling {
class DevicePoller;
enum class DeviceType;
} // namespace InputCommon::Polling

namespace InputCommon::SDL {

class State {
public:
    using Pollers = std::vector<std::unique_ptr<Polling::DevicePoller>>;

    /// Unregisters SDL device factories and shut them down.
    virtual ~State() = default;

    virtual Pollers GetPollers(Polling::DeviceType type) = 0;
};

class NullState : public State {
public:
    Pollers GetPollers(Polling::DeviceType type) override {
        return {};
    }
};

std::unique_ptr<State> Init();

} // namespace InputCommon::SDL
