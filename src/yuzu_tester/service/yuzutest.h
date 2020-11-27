// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>

namespace Core {
class System;
}

namespace Service::Yuzu {

struct TestResult {
    u32 code;
    std::string data;
    std::string name;
};

void InstallInterfaces(Core::System& system, std::string data,
                       std::function<void(std::vector<TestResult>)> finish_callback);

} // namespace Service::Yuzu
