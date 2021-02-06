// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/k_synchronization_object.h"
#include "core/hle/kernel/object.h"
#include "core/hle/result.h"

namespace Kernel {

class KernelCore;
class KEvent;

class KReadableEvent final : public KSynchronizationObject {
public:
    explicit KReadableEvent(KernelCore& kernel, std::string&& name);
    ~KReadableEvent() override;

    std::string GetTypeName() const override {
        return "KReadableEvent";
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::ReadableEvent;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    KEvent* GetParent() const {
        return parent;
    }

    void Initialize(KEvent* parent_) {
        is_signaled = false;
        parent = parent_;
    }

    bool IsSignaled() const override;
    void Finalize() override {}

    ResultCode Signal();
    ResultCode Clear();
    ResultCode Reset();

private:
    bool is_signaled{};
    KEvent* parent{};
};

} // namespace Kernel
