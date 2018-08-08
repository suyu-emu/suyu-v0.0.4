// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"
#include "profile_manager.h"

namespace Service::Account {

class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        explicit Interface(std::shared_ptr<Module> module, const char* name);

        void GetUserCount(Kernel::HLERequestContext& ctx);
        void GetUserExistence(Kernel::HLERequestContext& ctx);
        void ListAllUsers(Kernel::HLERequestContext& ctx);
        void ListOpenUsers(Kernel::HLERequestContext& ctx);
        void GetLastOpenedUser(Kernel::HLERequestContext& ctx);
        void GetProfile(Kernel::HLERequestContext& ctx);
        void InitializeApplicationInfo(Kernel::HLERequestContext& ctx);
        void GetBaasAccountManagerForApplication(Kernel::HLERequestContext& ctx);

    private:
        std::unique_ptr<ProfileManager> profile_manager{};

    protected:
        std::shared_ptr<Module> module;
    };
};

/// Registers all ACC services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace Service::Account
