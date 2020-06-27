// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>

namespace Common {

class SpinLock {
public:
    void lock();
    void unlock();
    bool try_lock();

private:
    std::atomic_flag lck = ATOMIC_FLAG_INIT;
};

} // namespace Common
