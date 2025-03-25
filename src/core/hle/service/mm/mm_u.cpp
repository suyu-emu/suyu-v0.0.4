// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/mm/mm_u.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/sm/sm.h"

#include <vector>

namespace Service::MM {
enum class Module : u32 {
    CPU = 0,
    GPU = 1,
    EMC = 2,
    SYS_BUS = 3,
    M_SELECT = 4,
    NVDEC = 5,
    NVENC = 6,
    NVJPG = 7,
    TEST = 8
};

class Session {
public:
    Session(Module module_, u32 request_id_, bool is_auto_clear_event_) {
        this->module = module_;
        this->request_id = request_id_;
        this->is_auto_clear_event = is_auto_clear_event_;
        this->min = 0;
        this->max = -1;
    };

public:
    Module module;
    u32 request_id, min;
    s32 max;
    bool is_auto_clear_event;

    void SetAndWait(u32 min_, s32 max_) {
        this->min = min_;
        this->max = max_;
    }
};

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
        LOG_DEBUG(Service_MM, "(STUBBED) called");

        IPC::RequestParser rp{ctx};
        const auto module = rp.PopEnum<Module>();
        rp.Pop<u32>();
        const auto event_clear_mode = rp.Pop<u32>();

        const bool is_auto_clear_event = event_clear_mode == 1;

        sessions.push_back({module, request_id++, is_auto_clear_event});

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void FinalizeOld(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called");

        IPC::RequestParser rp{ctx};
        const auto module = rp.PopEnum<Module>();

        for (auto it = sessions.begin(); it != sessions.end(); ++it) {
            if (it->module == module) {
                sessions.erase(it);
                break;
            }
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetAndWaitOld(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called");

        IPC::RequestParser rp{ctx};
        const auto module = rp.PopEnum<Module>();
        const auto min = rp.Pop<u32>();
        const auto max = rp.Pop<s32>();

        for (auto& session : sessions) {
            if (session.module == module) {
                session.SetAndWait(min, max);
                break;
            }
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetOld(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called");

        IPC::RequestParser rp{ctx};
        const auto module = rp.PopEnum<Module>();

        for (const auto& session : sessions) {
            if (session.module == module) {
                IPC::ResponseBuilder rb{ctx, 2};
                rb.Push(ResultSuccess);
                rb.Push(session.min);
                return;
            }
        }

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push<u32>(0);
    }

    void Initialize(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called");

        IPC::RequestParser rp{ctx};
        const auto module = rp.PopEnum<Module>();
        rp.Pop<u32>();
        const auto event_clear_mode = rp.Pop<u32>();

        const bool is_auto_clear_event = event_clear_mode == 1;

        sessions.push_back({module, request_id++, is_auto_clear_event});

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(request_id - 1);
    }

    void Finalize(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called");

        IPC::RequestParser rp{ctx};
        const auto id = rp.Pop<u32>();

        for (auto it = sessions.begin(); it != sessions.end(); ++it) {
            if (it->request_id == id) {
                sessions.erase(it);
                break;
            }
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetAndWait(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called");

        IPC::RequestParser rp{ctx};
        const auto id = rp.Pop<u32>();
        const auto min = rp.Pop<u32>();
        const auto max = rp.Pop<s32>();

        for (auto& session : sessions) {
            if (session.request_id == id) {
                session.SetAndWait(min, max);
                break;
            }
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void Get(HLERequestContext& ctx) {
        LOG_DEBUG(Service_MM, "(STUBBED) called");

        IPC::RequestParser rp{ctx};
        const auto id = rp.Pop<u32>();

        for (const auto& session : sessions) {
            if (session.request_id == id) {
                IPC::ResponseBuilder rb{ctx, 3};
                rb.Push(ResultSuccess);
                rb.Push(session.min);
                return;
            }
        }

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push<u32>(0);
    }

    std::vector<Session> sessions;
    u32 request_id{1};
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("mm:u", std::make_shared<MM_U>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::MM
