// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/slab_helpers.h"

namespace Kernel {

class KernelCore;
class KReadableEvent;
class KProcess;

class KEvent final : public KAutoObjectWithSlabHeapAndContainer<KEvent, KAutoObjectWithList> {
    KERNEL_AUTOOBJECT_TRAITS(KEvent, KAutoObject);

public:
    explicit KEvent(KernelCore& kernel);
    ~KEvent() override;

    void Initialize(KernelCore& kernel, KProcess* owner);

    void Finalize(KernelCore& kernel) override;

    bool IsInitialized() const override {
        return m_initialized;
    }

    uintptr_t GetPostDestroyArgument() const override {
        return reinterpret_cast<uintptr_t>(m_owner);
    }

    KProcess* GetOwner() const override {
        return m_owner;
    }

    KReadableEvent& GetReadableEvent() {
        return m_readable_event;
    }

    static void PostDestroy(KernelCore& kernel, uintptr_t arg);

    Result Signal(KernelCore& kernel);
    Result Clear(KernelCore& kernel);

    void OnReadableEventDestroyed() {
        m_readable_event_destroyed = true;
    }

private:
    KReadableEvent m_readable_event;
    KProcess* m_owner{};
    bool m_initialized{};
    bool m_readable_event_destroyed{};
};

} // namespace Kernel
