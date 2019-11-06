// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/object.h"

namespace Kernel {

class KernelCore;
class ReadableEvent;
class WritableEvent;

struct EventPair {
    SharedPtr<ReadableEvent> readable;
    SharedPtr<WritableEvent> writable;
};

class WritableEvent final : public Object {
public:
    ~WritableEvent() override;

    /**
     * Creates an event
     * @param kernel The kernel instance to create this event under.
     * @param name Optional name of event
     */
    static EventPair CreateEventPair(KernelCore& kernel, std::string name = "Unknown");

    std::string GetTypeName() const override {
        return "WritableEvent";
    }
    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::WritableEvent;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    SharedPtr<ReadableEvent> GetReadableEvent() const;

    void Signal();
    void Clear();
    bool IsSignaled() const;

private:
    explicit WritableEvent(KernelCore& kernel);

    SharedPtr<ReadableEvent> readable;

    std::string name; ///< Name of event (optional)
};

} // namespace Kernel
