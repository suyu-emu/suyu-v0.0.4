// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>

#include "common/common_types.h"
#include "common/spin_lock.h"

namespace Common {

class Fiber {
public:
    Fiber(std::function<void(void*)>&& entry_point_func, void* start_parameter);
    ~Fiber();

    Fiber(const Fiber&) = delete;
    Fiber& operator=(const Fiber&) = delete;

    Fiber(Fiber&&) = default;
    Fiber& operator=(Fiber&&) = default;

    /// Yields control from Fiber 'from' to Fiber 'to'
    /// Fiber 'from' must be the currently running fiber.
    static void YieldTo(std::shared_ptr<Fiber> from, std::shared_ptr<Fiber> to);
    static std::shared_ptr<Fiber> ThreadToFiber();

    /// Only call from main thread's fiber
    void Exit();

    /// Used internally but required to be public, Shall not be used
    void _start(void* parameter);

    /// Changes the start parameter of the fiber. Has no effect if the fiber already started
    void SetStartParameter(void* new_parameter) {
        start_parameter = new_parameter;
    }

private:
    Fiber();

    struct FiberImpl;

    SpinLock guard;
    std::function<void(void*)> entry_point;
    void* start_parameter;
    std::shared_ptr<Fiber> previous_fiber;
    std::unique_ptr<FiberImpl> impl;
    bool is_thread_fiber{};
};

} // namespace Common
