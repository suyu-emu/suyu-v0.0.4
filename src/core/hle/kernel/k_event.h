// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/object.h"

namespace Kernel {

class KernelCore;
class KReadableEvent;
class KWritableEvent;

class KEvent final : public Object {
public:
    explicit KEvent(KernelCore& kernel, std::string&& name);
    ~KEvent() override;

    static std::shared_ptr<KEvent> Create(KernelCore& kernel, std::string&& name);

    void Initialize();

    void Finalize() override {}

    std::string GetTypeName() const override {
        return "KEvent";
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::Event;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    std::shared_ptr<KReadableEvent>& GetReadableEvent() {
        return readable_event;
    }

    std::shared_ptr<KWritableEvent>& GetWritableEvent() {
        return writable_event;
    }

    const std::shared_ptr<KReadableEvent>& GetReadableEvent() const {
        return readable_event;
    }

    const std::shared_ptr<KWritableEvent>& GetWritableEvent() const {
        return writable_event;
    }

private:
    std::shared_ptr<KReadableEvent> readable_event;
    std::shared_ptr<KWritableEvent> writable_event;
    bool initialized{};
};

} // namespace Kernel
