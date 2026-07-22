// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "common/common_types.h"
#include "core/hle/kernel/k_synchronization_object.h"
#include "core/hle/result.h"

namespace Kernel {

class KLightClientSession;
class KClientSession;
class KernelCore;
class KPort;

class KClientPort final : public KSynchronizationObject {
    KERNEL_AUTOOBJECT_TRAITS(KClientPort, KSynchronizationObject);

public:
    explicit KClientPort(KernelCore& kernel);
    ~KClientPort() override;

    void Initialize(KernelCore& kernel, KPort* parent, s32 max_sessions);
    void OnSessionFinalized(KernelCore& kernel);
    void OnServerClosed(KernelCore& kernel);

    const KPort* GetParent() const {
        return m_parent;
    }
    KPort* GetParent() {
        return m_parent;
    }

    s32 GetNumSessions() const {
        return m_num_sessions;
    }
    s32 GetPeakSessions() const {
        return m_peak_sessions;
    }
    s32 GetMaxSessions() const {
        return m_max_sessions;
    }

    bool IsLight(KernelCore& kernel) const;
    bool IsServerClosed(KernelCore& kernel) const;

    // Overridden virtual functions.
    void Destroy(KernelCore& kernel) override;
    bool IsSignaled(KernelCore& kernel) const override;

    Result CreateSession(KernelCore& kernel, KClientSession** out);
    Result CreateLightSession(KernelCore& kernel, KLightClientSession** out);

private:
    std::atomic<s32> m_num_sessions{};
    std::atomic<s32> m_peak_sessions{};
    s32 m_max_sessions{};
    KPort* m_parent{};
};

} // namespace Kernel
