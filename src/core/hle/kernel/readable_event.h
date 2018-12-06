// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/object.h"
#include "core/hle/kernel/wait_object.h"

union ResultCode;

namespace Kernel {

class KernelCore;
class WritableEvent;

class ReadableEvent final : public WaitObject {
    friend class WritableEvent;

public:
    ~ReadableEvent() override;

    std::string GetTypeName() const override {
        return "ReadableEvent";
    }
    std::string GetName() const override {
        return name;
    }

    ResetType GetResetType() const {
        return reset_type;
    }

    static const HandleType HANDLE_TYPE = HandleType::ReadableEvent;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    bool ShouldWait(Thread* thread) const override;
    void Acquire(Thread* thread) override;

    void WakeupAllWaitingThreads() override;

    /// Unconditionally clears the readable event's state.
    void Clear();

    /// Clears the readable event's state if and only if it
    /// has already been signaled.
    ///
    /// @pre The event must be in a signaled state. If this event
    ///      is in an unsignaled state and this function is called,
    ///      then ERR_INVALID_STATE will be returned.
    ResultCode Reset();

private:
    explicit ReadableEvent(KernelCore& kernel);

    void Signal();

    ResetType reset_type;
    bool signaled;

    std::string name; ///< Name of event (optional)
};

} // namespace Kernel
