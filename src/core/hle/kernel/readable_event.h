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

    static const HandleType HANDLE_TYPE = HandleType::Event;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    bool ShouldWait(Thread* thread) const override;
    void Acquire(Thread* thread) override;

    void WakeupAllWaitingThreads() override;

    void AddWaitingThread(SharedPtr<Thread> thread) override;
    void RemoveWaitingThread(Thread* thread) override;

    void Signal();
    void Clear();

    SharedPtr<WritableEvent> PromoteToWritable() const {
        return writable_event;
    }

private:
    explicit ReadableEvent(KernelCore& kernel);

    SharedPtr<WritableEvent> writable_event; ///< WritableEvent associated with this ReadableEvent

    std::string name; ///< Name of event (optional)
};

} // namespace Kernel
