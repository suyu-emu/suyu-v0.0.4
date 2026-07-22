// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "common/intrusive_list.h"

#include "core/hle/kernel/k_light_server_session.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/k_synchronization_object.h"

namespace Kernel {

class KernelCore;
class KPort;
class SessionRequestHandler;

class KServerPort final : public KSynchronizationObject {
    KERNEL_AUTOOBJECT_TRAITS(KServerPort, KSynchronizationObject);

public:
    explicit KServerPort(KernelCore& kernel);
    ~KServerPort() override;

    void Initialize(KernelCore& kernel, KPort* parent);

    void EnqueueSession(KernelCore& kernel, KServerSession* session);
    void EnqueueSession(KernelCore& kernel, KLightServerSession* session);

    KServerSession* AcceptSession(KernelCore& kernel);
    KLightServerSession* AcceptLightSession(KernelCore& kernel);

    const KPort* GetParent() const {
        return m_parent;
    }

    bool IsLight(KernelCore& kernel) const;

    // Overridden virtual functions.
    void Destroy(KernelCore& kernel) override;
    bool IsSignaled(KernelCore& kernel) const override;

private:
    using SessionList = Common::IntrusiveListBaseTraits<KServerSession>::ListType;
    using LightSessionList = Common::IntrusiveListBaseTraits<KLightServerSession>::ListType;

    void CleanupSessions(KernelCore& kernel);

    SessionList m_session_list{};
    LightSessionList m_light_session_list{};
    KPort* m_parent{};
};

} // namespace Kernel
