// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "core/hle/kernel/k_auto_object.h"
#include "core/hle/kernel/k_synchronization_object.h"
#include "core/hle/kernel/slab_helpers.h"
#include "core/hle/result.h"

union ResultCode;

namespace Core::Memory {
class Memory;
}

namespace Core::Timing {
class CoreTiming;
}

namespace Kernel {

class KernelCore;
class KSession;
class KThread;

class KClientSession final
    : public KAutoObjectWithSlabHeapAndContainer<KClientSession, KAutoObjectWithList> {
    KERNEL_AUTOOBJECT_TRAITS(KClientSession, KAutoObject);

public:
    explicit KClientSession(KernelCore& kernel);
    virtual ~KClientSession();

    void Initialize(KSession* parent_, std::string&& name_) {
        // Set member variables.
        parent = parent_;
        name = std::move(name_);
    }

    virtual void Destroy() override;
    static void PostDestroy([[maybe_unused]] uintptr_t arg) {}

    constexpr KSession* GetParent() const {
        return parent;
    }

    ResultCode SendSyncRequest(KThread* thread, Core::Memory::Memory& memory,
                               Core::Timing::CoreTiming& core_timing);

    void OnServerClosed();

    // DEPRECATED

    static constexpr HandleType HANDLE_TYPE = HandleType::ClientSession;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    std::string GetTypeName() const override {
        return "ClientSession";
    }

    std::string GetName() const override {
        return name;
    }

private:
    /// The parent session, which links to the server endpoint.
    KSession* parent{};

    /// Name of the client session (optional)
    std::string name;
};

} // namespace Kernel
