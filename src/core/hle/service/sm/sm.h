// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "core/hle/kernel/object.h"
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

    std::shared_ptr<ServiceManager> service_manager;
};

constexpr ResultCode ERR_SERVICE_NOT_REGISTERED(-1);
constexpr ResultCode ERR_MAX_CONNECTIONS_REACHED(-1);
constexpr ResultCode ERR_INVALID_NAME_SIZE(-1);
constexpr ResultCode ERR_NAME_CONTAINS_NUL(-1);
constexpr ResultCode ERR_ALREADY_REGISTERED(-1);

class ServiceManager {
public:
    static void InstallInterfaces(std::shared_ptr<ServiceManager> self);

    ~ServiceManager();

    ResultVal<Kernel::SharedPtr<Kernel::ServerPort>> RegisterService(std::string name,
                                                                     unsigned int max_sessions);
    ResultVal<Kernel::SharedPtr<Kernel::ClientPort>> GetServicePort(const std::string& name);
    ResultVal<Kernel::SharedPtr<Kernel::ClientSession>> ConnectToService(const std::string& name);

    void InvokeControlRequest(Kernel::HLERequestContext& context);

private:
    std::weak_ptr<SM> sm_interface;
    std::unique_ptr<Controller> controller_interface;

    /// Map of registered services, retrieved using GetServicePort or ConnectToService.
    std::unordered_map<std::string, Kernel::SharedPtr<Kernel::ClientPort>> registered_services;
};

} // namespace Service::SM
