// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/kernel/k_light_client_session.h"
#include "core/hle/kernel/k_light_server_session.h"
#include "core/hle/kernel/slab_helpers.h"
#include "core/hle/result.h"

namespace Kernel {

class KClientPort;
class KProcess;

// TODO: SupportDynamicExpansion for SlabHeap
class KLightSession final
    : public KAutoObjectWithSlabHeapAndContainer<KLightSession, KAutoObjectWithList> {
    KERNEL_AUTOOBJECT_TRAITS(KLightSession, KAutoObject);

private:
    enum class State : u8 {
        Invalid = 0,
        Normal = 1,
        ClientClosed = 2,
        ServerClosed = 3,
    };

public:
    static constexpr size_t DataSize = sizeof(u32) * 7;
    static constexpr u32 ReplyFlag = (1U << 31);

private:
    KLightServerSession m_server;
    KLightClientSession m_client;
    State m_state{State::Invalid};
    KClientPort* m_port{};
    uintptr_t m_name{};
    KProcess* m_process{};
    bool m_initialized{};

public:
    explicit KLightSession(KernelCore& kernel);
    ~KLightSession();

    void Initialize(KernelCore& kernel, KClientPort* client_port, uintptr_t name);
    void Finalize(KernelCore& kernel) override;

    bool IsInitialized() const override {
        return m_initialized;
    }
    uintptr_t GetPostDestroyArgument() const override {
        return uintptr_t(m_process);
    }

    static void PostDestroy(KernelCore& kernel, uintptr_t arg);

    void OnServerClosed(KernelCore& kernel);
    void OnClientClosed(KernelCore& kernel);

    bool IsServerClosed() const {
        return m_state != State::Normal;
    }
    bool IsClientClosed() const {
        return m_state != State::Normal;
    }

    Result OnRequest(KernelCore& kernel, KThread* request_thread) {
        R_RETURN(m_server.OnRequest(kernel, request_thread));
    }

    KLightClientSession& GetClientSession() {
        return m_client;
    }
    KLightServerSession& GetServerSession() {
        return m_server;
    }
    const KLightClientSession& GetClientSession() const {
        return m_client;
    }
    const KLightServerSession& GetServerSession() const {
        return m_server;
    }
};

} // namespace Kernel
