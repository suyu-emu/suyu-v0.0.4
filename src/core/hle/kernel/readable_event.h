// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/object.h"
#include "core/hle/kernel/wait_object.h"

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

    void Clear();

private:
    explicit ReadableEvent(KernelCore& kernel);

    void Signal();

    ResetType reset_type;
    bool signaled;

    std::string name; ///< Name of event (optional)
};

} // namespace Kernel
