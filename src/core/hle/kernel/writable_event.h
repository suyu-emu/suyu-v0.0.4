// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "core/hle/kernel/object.h"

namespace Kernel {

class KernelCore;
class ReadableEvent;
class WritableEvent;

struct EventPair {
    std::shared_ptr<ReadableEvent> readable;
    std::shared_ptr<WritableEvent> writable;
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

    std::shared_ptr<ReadableEvent> GetReadableEvent() const;

    void Signal();
    void Clear();

    void Finalize() override {}

private:
    explicit WritableEvent(KernelCore& kernel);

    std::shared_ptr<ReadableEvent> readable;

    std::string name; ///< Name of event (optional)
};

} // namespace Kernel
