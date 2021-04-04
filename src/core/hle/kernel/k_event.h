// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/slab_helpers.h"

namespace Kernel {

class KernelCore;
class KReadableEvent;
class KWritableEvent;
class Process;

class KEvent final : public KAutoObjectWithSlabHeapAndContainer<KEvent, KAutoObjectWithList> {
    KERNEL_AUTOOBJECT_TRAITS(KEvent, KAutoObject);

public:
    explicit KEvent(KernelCore& kernel);
    ~KEvent() override;

    void Initialize(std::string&& name);

    virtual void Finalize() override;

    virtual bool IsInitialized() const override {
        return initialized;
    }
    virtual uintptr_t GetPostDestroyArgument() const override {
        return reinterpret_cast<uintptr_t>(owner);
    }

    static void PostDestroy(uintptr_t arg);

    std::string GetTypeName() const override {
        return "KEvent";
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::Event;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    KReadableEvent* GetReadableEvent() {
        return readable_event.get();
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
    Process* owner{};
    bool initialized{};
};

} // namespace Kernel
