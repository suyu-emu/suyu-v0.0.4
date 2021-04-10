// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_writable_event.h"
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

    virtual Process* GetOwner() const override {
        return owner;
    }

    KReadableEvent& GetReadableEvent() {
        return readable_event;
    }

    KWritableEvent& GetWritableEvent() {
        return writable_event;
    }

    // DEPRECATED

    std::string GetTypeName() const override {
        return "KEvent";
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::Event;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

private:
    KReadableEvent readable_event;
    KWritableEvent writable_event;
    Process* owner{};
    bool initialized{};
};

} // namespace Kernel
