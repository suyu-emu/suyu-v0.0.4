// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include "common/assert.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/result.h"
#include "core/hle/service/sm/controller.h"
#include "core/hle/service/sm/sm.h"

namespace Service::SM {

constexpr ResultCode ERR_ALREADY_REGISTERED(ErrorModule::SM, 4);
constexpr ResultCode ERR_INVALID_NAME(ErrorModule::SM, 6);
constexpr ResultCode ERR_SERVICE_NOT_REGISTERED(ErrorModule::SM, 7);

ServiceManager::ServiceManager() = default;
ServiceManager::~ServiceManager() = default;

void ServiceManager::InvokeControlRequest(Kernel::HLERequestContext& context) {
    controller_interface->InvokeRequest(context);
}

static ResultCode ValidateServiceName(const std::string& name) {
    if (name.size() <= 0 || name.size() > 8) {
        return ERR_INVALID_NAME;
    }
    if (name.find('\0') != std::string::npos) {
        return ERR_INVALID_NAME;
    }
    return RESULT_SUCCESS;
}

void ServiceManager::InstallInterfaces(std::shared_ptr<ServiceManager> self) {
    ASSERT(self->sm_interface.expired());

    auto sm = std::make_shared<SM>(self);
    sm->InstallAsNamedPort();
    self->sm_interface = sm;
    self->controller_interface = std::make_unique<Controller>();
}

ResultVal<Kernel::SharedPtr<Kernel::ServerPort>> ServiceManager::RegisterService(
    std::string name, unsigned int max_sessions) {

    CASCADE_CODE(ValidateServiceName(name));

    if (registered_services.find(name) != registered_services.end())
        return ERR_ALREADY_REGISTERED;

    auto& kernel = Core::System::GetInstance().Kernel();
    Kernel::SharedPtr<Kernel::ServerPort> server_port;
    Kernel::SharedPtr<Kernel::ClientPort> client_port;
    std::tie(server_port, client_port) =
        Kernel::ServerPort::CreatePortPair(kernel, max_sessions, name);

    registered_services.emplace(std::move(name), std::move(client_port));
    return MakeResult<Kernel::SharedPtr<Kernel::ServerPort>>(std::move(server_port));
}

ResultCode ServiceManager::UnregisterService(std::string name) {
    CASCADE_CODE(ValidateServiceName(name));

    const auto iter = registered_services.find(name);
    if (iter == registered_services.end())
        return ERR_SERVICE_NOT_REGISTERED;

    registered_services.erase(iter);
    return RESULT_SUCCESS;
}

ResultVal<Kernel::SharedPtr<Kernel::ClientPort>> ServiceManager::GetServicePort(
    const std::string& name) {

    CASCADE_CODE(ValidateServiceName(name));
    auto it = registered_services.find(name);
    if (it == registered_services.end()) {
        return ERR_SERVICE_NOT_REGISTERED;
    }

    return MakeResult<Kernel::SharedPtr<Kernel::ClientPort>>(it->second);
}

ResultVal<Kernel::SharedPtr<Kernel::ClientSession>> ServiceManager::ConnectToService(
    const std::string& name) {

    CASCADE_RESULT(auto client_port, GetServicePort(name));
    return client_port->Connect();
}

SM::~SM() = default;

/**
 * SM::Initialize service function
 *  Inputs:
 *      0: 0x00000000
 *  Outputs:
 *      0: ResultCode
 */
void SM::Initialize(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SM, "called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
}

void SM::GetService(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto name_buf = rp.PopRaw<std::array<char, 8>>();
    auto end = std::find(name_buf.begin(), name_buf.end(), '\0');

    std::string name(name_buf.begin(), end);

    // TODO(yuriks): Permission checks go here

    auto client_port = service_manager->GetServicePort(name);
    if (client_port.Failed()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(client_port.Code());
        LOG_ERROR(Service_SM, "called service={} -> error 0x{:08X}", name, client_port.Code().raw);
        if (name.length() == 0)
            return; // LibNX Fix
        UNIMPLEMENTED();
        return;
    }

    auto session = client_port.Unwrap()->Connect();
    ASSERT(session.Succeeded());
    if (session.Succeeded()) {
        LOG_DEBUG(Service_SM, "called service={} -> session={}", name, (*session)->GetObjectId());
        IPC::ResponseBuilder rb{ctx, 2, 0, 1, IPC::ResponseBuilder::Flags::AlwaysMoveHandles};
        rb.Push(session.Code());
        rb.PushMoveObjects(std::move(session).Unwrap());
    }
}

void SM::RegisterService(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto name_buf = rp.PopRaw<std::array<char, 8>>();
    const auto end = std::find(name_buf.begin(), name_buf.end(), '\0');

    const std::string name(name_buf.begin(), end);

    const auto unk_bool = static_cast<bool>(rp.PopRaw<u32>());
    const auto session_count = rp.PopRaw<u32>();

    LOG_DEBUG(Service_SM, "called with unk_bool={}", unk_bool);

    auto handle = service_manager->RegisterService(name, session_count);
    if (handle.Failed()) {
        LOG_ERROR(Service_SM, "failed to register service with error_code={:08X}",
                  handle.Code().raw);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(handle.Code());
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 0, 1, IPC::ResponseBuilder::Flags::AlwaysMoveHandles};
    rb.Push(handle.Code());
    rb.PushMoveObjects(std::move(handle).Unwrap());
}

void SM::UnregisterService(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto name_buf = rp.PopRaw<std::array<char, 8>>();
    const auto end = std::find(name_buf.begin(), name_buf.end(), '\0');

    const std::string name(name_buf.begin(), end);
    LOG_DEBUG(Service_SM, "called with name={}", name);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(service_manager->UnregisterService(name));
}

SM::SM(std::shared_ptr<ServiceManager> service_manager)
    : ServiceFramework("sm:", 4), service_manager(std::move(service_manager)) {
    static const FunctionInfo functions[] = {
        {0x00000000, &SM::Initialize, "Initialize"},
        {0x00000001, &SM::GetService, "GetService"},
        {0x00000002, &SM::RegisterService, "RegisterService"},
        {0x00000003, &SM::UnregisterService, "UnregisterService"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::SM
