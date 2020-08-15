// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "common/param_package.h"

namespace InputCommon::CemuhookUDP {

class Client;
class UDPMotionFactory;
class UDPTouchFactory;

class State {
public:
    State();
    ~State();
    void ReloadUDPClient();
    std::vector<Common::ParamPackage> GetInputDevices() const;

private:
    std::unique_ptr<Client> client;
    std::shared_ptr<UDPMotionFactory> motion_factory;
    std::shared_ptr<UDPTouchFactory> touch_factory;
};

std::unique_ptr<State> Init();

} // namespace InputCommon::CemuhookUDP
