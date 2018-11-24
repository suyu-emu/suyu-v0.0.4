// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Kernel {
class ClientPort;
class ClientSession;
class ServerPort;
class SessionRequestHandler;
} // namespace Kernel

namespace Service::SM {

class Controller;

/// Interface to "sm:" service
class SM final : public ServiceFramework<SM> {
public:
    explicit SM(std::shared_ptr<ServiceManager> service_manager);
    ~SM() override;

private:
    void Initialize(Kernel::HLERequestContext& ctx);
    void GetService(Kernel::HLERequestContext& ctx);
    void RegisterService(Kernel::HLERequestContext& ctx);
    void UnregisterService(Kernel::HLERequestContext& ctx);

    std::shared_ptr<ServiceManager> service_manager;
};

class ServiceManager {
public:
    static void InstallInterfaces(std::shared_ptr<ServiceManager> self);

    ServiceManager();
    ~ServiceManager();

    ResultVal<Kernel::SharedPtr<Kernel::ServerPort>> RegisterService(std::string name,
                                                                     unsigned int max_sessions);
    ResultCode UnregisterService(const std::string& name);
    ResultVal<Kernel::SharedPtr<Kernel::ClientPort>> GetServicePort(const std::string& name);
    ResultVal<Kernel::SharedPtr<Kernel::ClientSession>> ConnectToService(const std::string& name);

    template <typename T>
    std::shared_ptr<T> GetService(const std::string& service_name) const {
        static_assert(std::is_base_of_v<Kernel::SessionRequestHandler, T>,
                      "Not a base of ServiceFrameworkBase");
        auto service = registered_services.find(service_name);
        if (service == registered_services.end()) {
            LOG_DEBUG(Service, "Can't find service: {}", service_name);
            return nullptr;
        }
        auto port = service->second->GetServerPort();
        if (port == nullptr) {
            return nullptr;
        }
        return std::static_pointer_cast<T>(port->hle_handler);
    }

    void InvokeControlRequest(Kernel::HLERequestContext& context);

private:
    std::weak_ptr<SM> sm_interface;
    std::unique_ptr<Controller> controller_interface;

    /// Map of registered services, retrieved using GetServicePort or ConnectToService.
    std::unordered_map<std::string, Kernel::SharedPtr<Kernel::ClientPort>> registered_services;
};

} // namespace Service::SM
