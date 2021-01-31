// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/object.h"
#include "core/hle/result.h"

namespace Kernel {

class KernelCore;
class KEvent;

class KWritableEvent final : public Object {
public:
    explicit KWritableEvent(KernelCore& kernel, std::string&& name);
    ~KWritableEvent() override;

    std::string GetTypeName() const override {
        return "KWritableEvent";
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::WritableEvent;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    void Initialize(KEvent* parent_);

    void Finalize() override {}

    ResultCode Signal();
    ResultCode Clear();

    KEvent* GetParent() const {
        return parent;
    }

private:
    KEvent* parent{};
};

} // namespace Kernel
