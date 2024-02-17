// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "common/settings.h"
#include "core/arm/debug.h"
#include "core/core.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/glue/glue_manager.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/ns/account_proxy_interface.h"
#include "core/hle/service/ns/application_manager_interface.h"
#include "core/hle/service/ns/application_version_interface.h"
#include "core/hle/service/ns/content_management_interface.h"
#include "core/hle/service/ns/develop_interface.h"
#include "core/hle/service/ns/document_interface.h"
#include "core/hle/service/ns/download_task_interface.h"
#include "core/hle/service/ns/dynamic_rights_interface.h"
#include "core/hle/service/ns/ecommerce_interface.h"
#include "core/hle/service/ns/factory_reset_interface.h"
#include "core/hle/service/ns/language.h"
#include "core/hle/service/ns/ns.h"
#include "core/hle/service/ns/ns_results.h"
#include "core/hle/service/ns/pdm_qry.h"
#include "core/hle/service/ns/platform_service_manager.h"
#include "core/hle/service/ns/read_only_application_control_data_interface.h"
#include "core/hle/service/ns/read_only_application_record_interface.h"
#include "core/hle/service/ns/system_update_control.h"
#include "core/hle/service/ns/system_update_interface.h"
#include "core/hle/service/ns/vulnerability_manager_interface.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/set/settings_server.h"

namespace Service::NS {

NS::NS(const char* name, Core::System& system_) : ServiceFramework{system_, name} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {7988, &NS::PushInterface<IDynamicRightsInterface>, "GetDynamicRightsInterface"},
        {7989, &NS::PushInterface<IReadOnlyApplicationControlDataInterface>, "GetReadOnlyApplicationControlDataInterface"},
        {7991, &NS::PushInterface<IReadOnlyApplicationRecordInterface>, "GetReadOnlyApplicationRecordInterface"},
        {7992, &NS::PushInterface<IECommerceInterface>, "GetECommerceInterface"},
        {7993, &NS::PushInterface<IApplicationVersionInterface>, "GetApplicationVersionInterface"},
        {7994, &NS::PushInterface<IFactoryResetInterface>, "GetFactoryResetInterface"},
        {7995, &NS::PushInterface<IAccountProxyInterface>, "GetAccountProxyInterface"},
        {7996, &NS::PushIApplicationManagerInterface, "GetApplicationManagerInterface"},
        {7997, &NS::PushInterface<IDownloadTaskInterface>, "GetDownloadTaskInterface"},
        {7998, &NS::PushInterface<IContentManagementInterface>, "GetContentManagementInterface"},
        {7999, &NS::PushInterface<IDocumentInterface>, "GetDocumentInterface"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

NS::~NS() = default;

std::shared_ptr<IApplicationManagerInterface> NS::GetApplicationManagerInterface() const {
    return GetInterface<IApplicationManagerInterface>(system);
}

template <typename T, typename... Args>
void NS::PushInterface(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NS, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<T>(system);
}

void NS::PushIApplicationManagerInterface(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NS, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IApplicationManagerInterface>(system);
}

template <typename T, typename... Args>
std::shared_ptr<T> NS::GetInterface(Args&&... args) const {
    static_assert(std::is_base_of_v<SessionRequestHandler, T>,
                  "Not a base of ServiceFrameworkBase");

    return std::make_shared<T>(std::forward<Args>(args)...);
}

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("ns:am2", std::make_shared<NS>("ns:am2", system));
    server_manager->RegisterNamedService("ns:ec", std::make_shared<NS>("ns:ec", system));
    server_manager->RegisterNamedService("ns:rid", std::make_shared<NS>("ns:rid", system));
    server_manager->RegisterNamedService("ns:rt", std::make_shared<NS>("ns:rt", system));
    server_manager->RegisterNamedService("ns:web", std::make_shared<NS>("ns:web", system));
    server_manager->RegisterNamedService("ns:ro", std::make_shared<NS>("ns:ro", system));

    server_manager->RegisterNamedService("ns:dev", std::make_shared<IDevelopInterface>(system));
    server_manager->RegisterNamedService("ns:su", std::make_shared<ISystemUpdateInterface>(system));
    server_manager->RegisterNamedService("ns:vm",
                                         std::make_shared<IVulnerabilityManagerInterface>(system));
    server_manager->RegisterNamedService("pdm:qry", std::make_shared<PDM_QRY>(system));

    server_manager->RegisterNamedService("pl:s",
                                         std::make_shared<IPlatformServiceManager>(system, "pl:s"));
    server_manager->RegisterNamedService("pl:u",
                                         std::make_shared<IPlatformServiceManager>(system, "pl:u"));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::NS
