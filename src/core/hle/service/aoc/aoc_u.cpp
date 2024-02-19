// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <numeric>
#include <vector>

#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/common_funcs.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/aoc/aoc_u.h"
#include "core/hle/service/aoc/purchase_event_manager.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/server_manager.h"
#include "core/loader/loader.h"

namespace Service::AOC {

static bool CheckAOCTitleIDMatchesBase(u64 title_id, u64 base) {
    return FileSys::GetBaseTitleID(title_id) == base;
}

static std::vector<u64> AccumulateAOCTitleIDs(Core::System& system) {
    std::vector<u64> add_on_content;
    const auto& rcu = system.GetContentProvider();
    const auto list =
        rcu.ListEntriesFilter(FileSys::TitleType::AOC, FileSys::ContentRecordType::Data);
    std::transform(list.begin(), list.end(), std::back_inserter(add_on_content),
                   [](const FileSys::ContentProviderEntry& rce) { return rce.title_id; });
    add_on_content.erase(
        std::remove_if(
            add_on_content.begin(), add_on_content.end(),
            [&rcu](u64 tid) {
                return rcu.GetEntry(tid, FileSys::ContentRecordType::Data)->GetStatus() !=
                       Loader::ResultStatus::Success;
            }),
        add_on_content.end());
    return add_on_content;
}

AOC_U::AOC_U(Core::System& system_)
    : ServiceFramework{system_, "aoc:u"}, add_on_content{AccumulateAOCTitleIDs(system)},
      service_context{system_, "aoc:u"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "CountAddOnContentByApplicationId"},
        {1, nullptr, "ListAddOnContentByApplicationId"},
        {2, D<&AOC_U::CountAddOnContent>, "CountAddOnContent"},
        {3, D<&AOC_U::ListAddOnContent>, "ListAddOnContent"},
        {4, nullptr, "GetAddOnContentBaseIdByApplicationId"},
        {5, D<&AOC_U::GetAddOnContentBaseId>, "GetAddOnContentBaseId"},
        {6, nullptr, "PrepareAddOnContentByApplicationId"},
        {7, D<&AOC_U::PrepareAddOnContent>, "PrepareAddOnContent"},
        {8, D<&AOC_U::GetAddOnContentListChangedEvent>, "GetAddOnContentListChangedEvent"},
        {9, nullptr, "GetAddOnContentLostErrorCode"},
        {10, D<&AOC_U::GetAddOnContentListChangedEventWithProcessId>, "GetAddOnContentListChangedEventWithProcessId"},
        {11, D<&AOC_U::NotifyMountAddOnContent>, "NotifyMountAddOnContent"},
        {12, D<&AOC_U::NotifyUnmountAddOnContent>, "NotifyUnmountAddOnContent"},
        {13, nullptr, "IsAddOnContentMountedForDebug"},
        {50, D<&AOC_U::CheckAddOnContentMountStatus>, "CheckAddOnContentMountStatus"},
        {100, D<&AOC_U::CreateEcPurchasedEventManager>, "CreateEcPurchasedEventManager"},
        {101, D<&AOC_U::CreatePermanentEcPurchasedEventManager>, "CreatePermanentEcPurchasedEventManager"},
        {110, nullptr, "CreateContentsServiceManager"},
        {200, nullptr, "SetRequiredAddOnContentsOnContentsAvailabilityTransition"},
        {300, nullptr, "SetupHostAddOnContent"},
        {301, nullptr, "GetRegisteredAddOnContentPath"},
        {302, nullptr, "UpdateCachedList"},
    };
    // clang-format on

    RegisterHandlers(functions);

    aoc_change_event = service_context.CreateEvent("GetAddOnContentListChanged:Event");
}

AOC_U::~AOC_U() {
    service_context.CloseEvent(aoc_change_event);
}

Result AOC_U::CountAddOnContent(Out<u32> out_count, ClientProcessId process_id) {
    LOG_DEBUG(Service_AOC, "called. process_id={}", process_id.pid);

    const auto current = system.GetApplicationProcessProgramID();

    const auto& disabled = Settings::values.disabled_addons[current];
    if (std::find(disabled.begin(), disabled.end(), "DLC") != disabled.end()) {
        *out_count = 0;
        R_SUCCEED();
    }

    *out_count = static_cast<u32>(
        std::count_if(add_on_content.begin(), add_on_content.end(),
                      [current](u64 tid) { return CheckAOCTitleIDMatchesBase(tid, current); }));

    R_SUCCEED();
}

Result AOC_U::ListAddOnContent(Out<u32> out_count, OutBuffer<BufferAttr_HipcMapAlias> out_addons,
                               u32 offset, u32 count, ClientProcessId process_id) {
    LOG_DEBUG(Service_AOC, "called with offset={}, count={}, process_id={}", offset, count,
              process_id.pid);

    const auto current = FileSys::GetBaseTitleID(system.GetApplicationProcessProgramID());

    std::vector<u32> out;
    const auto& disabled = Settings::values.disabled_addons[current];
    if (std::find(disabled.begin(), disabled.end(), "DLC") == disabled.end()) {
        for (u64 content_id : add_on_content) {
            if (FileSys::GetBaseTitleID(content_id) != current) {
                continue;
            }

            out.push_back(static_cast<u32>(FileSys::GetAOCID(content_id)));
        }
    }

    // TODO(DarkLordZach): Find the correct error code.
    R_UNLESS(out.size() >= offset, ResultUnknown);

    *out_count = static_cast<u32>(std::min<size_t>(out.size() - offset, count));
    std::rotate(out.begin(), out.begin() + offset, out.end());

    std::memcpy(out_addons.data(), out.data(), *out_count);

    R_SUCCEED();
}

Result AOC_U::GetAddOnContentBaseId(Out<u64> out_title_id, ClientProcessId process_id) {
    LOG_DEBUG(Service_AOC, "called. process_id={}", process_id.pid);

    const auto title_id = system.GetApplicationProcessProgramID();
    const FileSys::PatchManager pm{title_id, system.GetFileSystemController(),
                                   system.GetContentProvider()};

    const auto res = pm.GetControlMetadata();
    if (res.first == nullptr) {
        *out_title_id = FileSys::GetAOCBaseTitleID(title_id);
        R_SUCCEED();
    }

    *out_title_id = res.first->GetDLCBaseTitleId();

    R_SUCCEED();
}

Result AOC_U::PrepareAddOnContent(s32 addon_index, ClientProcessId process_id) {
    LOG_WARNING(Service_AOC, "(STUBBED) called with addon_index={}, process_id={}", addon_index,
                process_id.pid);

    R_SUCCEED();
}

Result AOC_U::GetAddOnContentListChangedEvent(OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_WARNING(Service_AOC, "(STUBBED) called");

    *out_event = &aoc_change_event->GetReadableEvent();

    R_SUCCEED();
}

Result AOC_U::GetAddOnContentListChangedEventWithProcessId(
    OutCopyHandle<Kernel::KReadableEvent> out_event, ClientProcessId process_id) {
    LOG_WARNING(Service_AOC, "(STUBBED) called");

    *out_event = &aoc_change_event->GetReadableEvent();

    R_SUCCEED();
}

Result AOC_U::NotifyMountAddOnContent() {
    LOG_WARNING(Service_AOC, "(STUBBED) called");

    R_SUCCEED();
}

Result AOC_U::NotifyUnmountAddOnContent() {
    LOG_WARNING(Service_AOC, "(STUBBED) called");

    R_SUCCEED();
}

Result AOC_U::CheckAddOnContentMountStatus() {
    LOG_WARNING(Service_AOC, "(STUBBED) called");

    R_SUCCEED();
}

Result AOC_U::CreateEcPurchasedEventManager(OutInterface<IPurchaseEventManager> out_interface) {
    LOG_WARNING(Service_AOC, "(STUBBED) called");

    *out_interface = std::make_shared<IPurchaseEventManager>(system);

    R_SUCCEED();
}

Result AOC_U::CreatePermanentEcPurchasedEventManager(
    OutInterface<IPurchaseEventManager> out_interface) {
    LOG_WARNING(Service_AOC, "(STUBBED) called");

    *out_interface = std::make_shared<IPurchaseEventManager>(system);

    R_SUCCEED();
}

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);
    server_manager->RegisterNamedService("aoc:u", std::make_shared<AOC_U>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::AOC
