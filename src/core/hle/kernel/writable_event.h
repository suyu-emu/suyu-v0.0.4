// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/wait_object.h"

namespace Kernel {

class KernelCore;
class ReadableEvent;

class WritableEvent final : public WaitObject {
public:
    ~WritableEvent() override;

    /**
     * Creates an event
     * @param kernel The kernel instance to create this event under.
     * @param reset_type ResetType describing how to create event
     * @param name Optional name of event
     */
    static std::tuple<SharedPtr<WritableEvent>, SharedPtr<ReadableEvent>> CreateEventPair(
        KernelCore& kernel, ResetType reset_type, std::string name = "Unknown");

    /**
     * Creates an event and registers it in the kernel's named event table
     * @param kernel The kernel instance to create this event under.
     * @param reset_type ResetType describing how to create event
     * @param name name of event
     */
    static SharedPtr<WritableEvent> CreateRegisteredEventPair(KernelCore& kernel,
                                                              ResetType reset_type,
                                                              std::string name);

    std::string GetTypeName() const override {
        return "WritableEvent";
    }
    std::string GetName() const override {
        return name;
    }

    static const HandleType HANDLE_TYPE = HandleType::Event;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    ResetType GetResetType() const {
        return reset_type;
    }

    bool ShouldWait(Thread* thread) const override;
    void Acquire(Thread* thread) override;

    void WakeupAllWaitingThreads() override;

    void Signal();
    void Clear();
    void ResetOnAcquire();
    void ResetOnWakeup();
    bool IsSignaled() const;

private:
    explicit WritableEvent(KernelCore& kernel);

    ResetType reset_type; ///< Current ResetType

    bool signaled;    ///< Whether the event has already been signaled
    std::string name; ///< Name of event (optional)
};

} // namespace Kernel
