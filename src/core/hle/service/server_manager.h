// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <string_view>
#include <vector>

#include "common/polyfill_thread.h"
#include "common/thread.h"
#include "core/hle/result.h"
#include "core/hle/service/hle_ipc.h"
#include "core/hle/service/os/mutex.h"

namespace Core {
class System;
}

namespace Kernel {
class KEvent;
class KServerPort;
class KServerSession;
class KSynchronizationObject;
} // namespace Kernel

namespace Service {

class ServerManager {
public:
    explicit ServerManager(Core::System& system);
    ~ServerManager();

    Result RegisterSession(Kernel::KServerSession* session,
                           std::shared_ptr<SessionRequestManager> manager);
    Result RegisterNamedService(const std::string& service_name,
                                SessionRequestHandlerFactory&& handler_factory,
                                u32 max_sessions = 64);
    Result RegisterNamedService(const std::string& service_name,
                                std::shared_ptr<SessionRequestHandler>&& handler,
                                u32 max_sessions = 64);
    Result ManageNamedPort(const std::string& service_name,
                           SessionRequestHandlerFactory&& handler_factory, u32 max_sessions = 64);
    Result ManageDeferral(Kernel::KEvent** out_event);

    Result LoopProcess();
    void StartAdditionalHostThreads(const char* name, size_t num_threads);

    static void RunServer(std::unique_ptr<ServerManager>&& server);

private:
    struct RequestState;

    Result LoopProcessImpl();
    Result WaitAndProcessImpl();
    Result OnPortEvent(Kernel::KServerPort* port, SessionRequestHandlerFactory&& handler_factory);
    Result OnSessionEvent(Kernel::KServerSession* session,
                          std::shared_ptr<SessionRequestManager>&& manager);
    Result OnDeferralEvent(std::list<RequestState>&& deferrals);
    Result CompleteSyncRequest(RequestState&& state);

private:
    Core::System& m_system;
    Mutex m_serve_mutex;
    std::mutex m_list_mutex;

    // Guest state tracking
    std::map<Kernel::KServerPort*, SessionRequestHandlerFactory> m_ports{};
    std::map<Kernel::KServerSession*, std::shared_ptr<SessionRequestManager>> m_sessions{};
    Kernel::KEvent* m_event{};
    Kernel::KEvent* m_deferral_event{};

    // Deferral tracking
    struct RequestState {
        Kernel::KServerSession* session;
        std::shared_ptr<HLERequestContext> context;
        std::shared_ptr<SessionRequestManager> manager;
    };
    std::list<RequestState> m_deferrals{};

    // Host state tracking
    Common::Event m_stopped{};
    std::vector<std::jthread> m_threads{};
    std::stop_source m_stop_source{};
};

} // namespace Service
