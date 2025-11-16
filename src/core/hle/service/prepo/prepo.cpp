// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/hex_util.h"
#include "common/logging/log.h"
#include "common/uuid.h"
#include <cstring>

#include "core/core.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/prepo/prepo.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"
#include "core/reporter.h"

namespace Service::PlayReport {

class PlayReport final : public ServiceFramework<PlayReport> {
public:
    explicit PlayReport(const char* name, Core::System& system_) : ServiceFramework{system_, name} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {10100, &PlayReport::SaveReport<Core::Reporter::PlayReportType::Old>, "SaveReportOld"},
            {10101, &PlayReport::SaveReportWithUser<Core::Reporter::PlayReportType::Old>, "SaveReportWithUserOld"},
            {10102, &PlayReport::SaveReport<Core::Reporter::PlayReportType::Old2>, "SaveReportOld2"},
            {10103, &PlayReport::SaveReportWithUser<Core::Reporter::PlayReportType::Old2>, "SaveReportWithUserOld2"},
            {10104, &PlayReport::SaveReport<Core::Reporter::PlayReportType::New>, "SaveReport"},
            {10105, &PlayReport::SaveReportWithUser<Core::Reporter::PlayReportType::New>, "SaveReportWithUser"},
            {10200, &PlayReport::RequestImmediateTransmission, "RequestImmediateTransmission"},
            {10300, &PlayReport::GetTransmissionStatus, "GetTransmissionStatus"},
            {10400, &PlayReport::GetSystemSessionId, "GetSystemSessionId"},
            {20100, &PlayReport::SaveSystemReportOld, "SaveSystemReport"},
{20101, &PlayReport::SaveSystemReportWithUserOld, "SaveSystemReportWithUser"},
{20102, &PlayReport::SaveSystemReport, "SaveSystemReport"},
{20103, &PlayReport::SaveSystemReportWithUser, "SaveSystemReportWithUser"},
            {20200, nullptr, "SetOperationMode"},
            {30100, nullptr, "ClearStorage"},
            {30200, nullptr, "ClearStatistics"},
            {30300, nullptr, "GetStorageUsage"},
            {30400, nullptr, "GetStatistics"},
            {30401, nullptr, "GetThroughputHistory"},
            {30500, nullptr, "GetLastUploadError"},
            {30600, nullptr, "GetApplicationUploadSummary"},
            {40100, nullptr, "IsUserAgreementCheckEnabled"},
            {40101, nullptr, "SetUserAgreementCheckEnabled"},
            {50100, nullptr, "ReadAllApplicationReportFiles"},
            {90100, nullptr, "ReadAllReportFiles"},
            {90101, nullptr, "Unknown90101"},
            {90102, nullptr, "Unknown90102"},
            {90200, nullptr, "GetStatistics"},
            {90201, nullptr, "GetThroughputHistory"},
            {90300, nullptr, "GetLastUploadError"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    template <Core::Reporter::PlayReportType Type>
    void SaveReport(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto process_id = rp.PopRaw<u64>();

        const auto data1 = ctx.ReadBufferA(0);
        const auto data2 = ctx.ReadBufferX(0);

        LOG_DEBUG(Service_PREPO,
                  "called, type={:02X}, process_id={:016X}, data1_size={:016X}, data2_size={:016X}",
                  Type, process_id, data1.size(), data2.size());

        const auto& reporter{system.GetReporter()};
        reporter.SavePlayReport(Type, system.GetApplicationProcessProgramID(), {data1, data2},
                                process_id);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    template <Core::Reporter::PlayReportType Type>
    void SaveReportWithUser(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto user_id = rp.PopRaw<u128>();
        const auto process_id = rp.PopRaw<u64>();

        const auto data1 = ctx.ReadBufferA(0);
        const auto data2 = ctx.ReadBufferX(0);

        LOG_DEBUG(Service_PREPO,
                  "called, type={:02X}, user_id={:016X}{:016X}, process_id={:016X}, "
                  "data1_size={:016X}, data2_size={:016X}",
                  Type, user_id[1], user_id[0], process_id, data1.size(), data2.size());

        const auto& reporter{system.GetReporter()};
        reporter.SavePlayReport(Type, system.GetApplicationProcessProgramID(), {data1, data2},
                                process_id, user_id);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void RequestImmediateTransmission(HLERequestContext& ctx) {
        LOG_WARNING(Service_PREPO, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetTransmissionStatus(HLERequestContext& ctx) {
        LOG_WARNING(Service_PREPO, "(STUBBED) called");

        constexpr s32 status = 0;

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(status);
    }

    void GetSystemSessionId(HLERequestContext& ctx) {
        LOG_WARNING(Service_PREPO, "(STUBBED) called");

        constexpr u64 system_session_id = 0;
        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(ResultSuccess);
        rb.Push(system_session_id);
    }

    void SaveSystemReportOld(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto title_id = rp.PopRaw<u64>();

        const auto data1 = ctx.ReadBufferA(0);
        const auto data2 = ctx.ReadBufferX(0);

        LOG_DEBUG(Service_PREPO, "called, title_id={:016X}, data1_size={:016X}, data2_size={:016X}",
                  title_id, data1.size(), data2.size());

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SaveSystemReportWithUserOld(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto user_id = rp.PopRaw<u128>();
        const auto title_id = rp.PopRaw<u64>();

        const auto data1 = ctx.ReadBufferA(0);
        const auto data2 = ctx.ReadBufferX(0);

        Common::UUID uuid{};
        std::memcpy(uuid.uuid.data(), user_id.data(), sizeof(Common::UUID));

        LOG_DEBUG(Service_PREPO,
                  "called, user_id={}, title_id={:016X}, data1_size={:016X}, data2_size={:016X}",
                  uuid.FormattedString(), title_id, data1.size(), data2.size());

        const auto& reporter{system.GetReporter()};
        reporter.SavePlayReport(Core::Reporter::PlayReportType::System, title_id, {data1, data2},
                                std::nullopt, user_id);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    // (21.0.0+) buffers: [0x9 (X), 0x5 (A)], inbytes: 0x10
    void SaveSystemReport(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto field0 = rp.PopRaw<u64>();
        const auto title_id = rp.PopRaw<u64>();

        const auto data_x = ctx.ReadBufferX(0);
        const auto data_a = ctx.ReadBufferA(0);

        LOG_DEBUG(Service_PREPO,
                  "called, field0={}, title_id={:016X}, data_a_size={}, data_x_size={}",
                  field0, title_id, data_a.size(), data_x.size());

        const auto& reporter{system.GetReporter()};
        reporter.SavePlayReport(Core::Reporter::PlayReportType::System, title_id, {data_a, data_x});

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    // (21.0.0+) buffers: [0x9 (X), 0x5 (A)], inbytes: 0x20
    void SaveSystemReportWithUser(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        // 21.0.0+: field0 (u64), user_id (u128), title_id (u64)
        const auto field0 = rp.PopRaw<u64>();
        const auto user_id = rp.PopRaw<u128>();
        const auto title_id = rp.PopRaw<u64>();

        const auto data_x = ctx.ReadBufferX(0);
        const auto data_a = ctx.ReadBufferA(0);

        Common::UUID uuid{};
        std::memcpy(uuid.uuid.data(), user_id.data(), sizeof(Common::UUID));

        LOG_DEBUG(Service_PREPO,
                  "called, user_id={}, field0={:016X}, title_id={:016X}, data_a_size={}, data_x_size={}",
                  uuid.FormattedString(), field0, title_id, data_a.size(), data_x.size());

        const auto& reporter{system.GetReporter()};
        reporter.SavePlayReport(Core::Reporter::PlayReportType::System, title_id, {data_a, data_x},
                                std::nullopt, user_id);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("prepo:a",
                                         std::make_shared<PlayReport>("prepo:a", system));
    server_manager->RegisterNamedService("prepo:a2",
                                         std::make_shared<PlayReport>("prepo:a2", system));
    server_manager->RegisterNamedService("prepo:m",
                                         std::make_shared<PlayReport>("prepo:m", system));
    server_manager->RegisterNamedService("prepo:s",
                                         std::make_shared<PlayReport>("prepo:s", system));
    server_manager->RegisterNamedService("prepo:u",
                                         std::make_shared<PlayReport>("prepo:u", system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::PlayReport
