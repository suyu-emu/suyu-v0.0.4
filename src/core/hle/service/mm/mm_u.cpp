// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/mm/mm_u.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/sm/sm.h"

#include <vector>

namespace Service::MM {

namespace {

class Session {
public:
    Session(Module module_, u32 request_id_, bool auto_clear_event_)
        : module{module_}, request_id{request_id_}, auto_clear_event{auto_clear_event_} {}

    void SetAndWait(Setting minimum, s32 maximum_) {
        min = minimum;
        max = maximum_;
    }

    Module module;
    u32 request_id;
    bool auto_clear_event;
    Setting min{0};
    s32 max{-1};
};

} // Anonymous namespace

class MM_U final : public ServiceFramework<MM_U> {
public:
    explicit MM_U(Core::System& system_) : ServiceFramework{system_, "mm:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &MM_U::InitializeOld, "InitializeOld"},
            {1, &MM_U::FinalizeOld, "FinalizeOld"},
            {2, &MM_U::SetAndWaitOld, "SetAndWaitOld"},
            {3, &MM_U::GetOld, "GetOld"},
            {4, &MM_U::Initialize, "Initialize"},
            {5, &MM_U::Finalize, "Finalize"},
            {6, &MM_U::SetAndWait, "SetAndWait"},
            {7, &MM_U::Get, "Get"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void InitializeOld(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto module = rp.PopEnum<Module>();
        const auto priority = rp.Pop<Priority>();
        const auto event_clear_mode = rp.Pop<u32>();
        const bool auto_clear_event = event_clear_mode == 1;

        sessions.emplace_back(module, next_request_id++, auto_clear_event);
        LOG_DEBUG(Service_MM, "called, module={}, priority={}, auto_clear_event={}",
                  static_cast<u32>(module), priority, auto_clear_event);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void FinalizeOld(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto module = rp.PopEnum<Module>();

        std::erase_if(sessions, [module](const Session& session) {
            return session.module == module;
        });
        LOG_DEBUG(Service_MM, "called, module={}", static_cast<u32>(module));

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetAndWaitOld(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto module = rp.PopEnum<Module>();
        const auto min = rp.Pop<Setting>();
        const auto max = rp.Pop<s32>();

        for (auto& session : sessions) {
            if (session.module == module) {
                session.SetAndWait(min, max);
                break;
            }
        }
        LOG_DEBUG(Service_MM, "called, module={}, min={}, max={}", static_cast<u32>(module), min,
                  max);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetOld(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto module = rp.PopEnum<Module>();

        for (const auto& session : sessions) {
            if (session.module == module) {
                IPC::ResponseBuilder rb{ctx, 3};
                rb.Push(ResultSuccess);
                rb.Push(session.min);
                LOG_DEBUG(Service_MM, "called, module={}, current={}", static_cast<u32>(module),
                          session.min);
                return;
            }
        }

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push<Setting>(0);
        LOG_DEBUG(Service_MM, "called, module={}, current=0", static_cast<u32>(module));
    }

    void Initialize(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto module = rp.PopEnum<Module>();
        const auto priority = rp.Pop<Priority>();
        const auto event_clear_mode = rp.Pop<u32>();
        const bool auto_clear_event = event_clear_mode == 1;

        const auto request_id = next_request_id++;
        sessions.emplace_back(module, request_id, auto_clear_event);
        LOG_DEBUG(Service_MM, "called, module={}, priority={}, request_id={}, auto_clear_event={}",
                  static_cast<u32>(module), priority, request_id, auto_clear_event);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(request_id);
    }

    void Finalize(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto request_id = rp.Pop<u32>();

        std::erase_if(sessions, [request_id](const Session& session) {
            return session.request_id == request_id;
        });
        LOG_DEBUG(Service_MM, "called, request_id={}", request_id);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetAndWait(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto request_id = rp.Pop<u32>();
        const auto min = rp.Pop<Setting>();
        const auto max = rp.Pop<s32>();

        for (auto& session : sessions) {
            if (session.request_id == request_id) {
                session.SetAndWait(min, max);
                break;
            }
        }
        LOG_DEBUG(Service_MM, "called, request_id={}, min={}, max={}", request_id, min, max);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void Get(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto request_id = rp.Pop<u32>();

        for (const auto& session : sessions) {
            if (session.request_id == request_id) {
                IPC::ResponseBuilder rb{ctx, 3};
                rb.Push(ResultSuccess);
                rb.Push(session.min);
                LOG_DEBUG(Service_MM, "called, request_id={}, current={}", request_id,
                          session.min);
                return;
            }
        }

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push<Setting>(0);
        LOG_DEBUG(Service_MM, "called, request_id={}, current=0", request_id);
    }

    std::vector<Session> sessions;
    u32 next_request_id{1};
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("mm:u", std::make_shared<MM_U>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::MM
