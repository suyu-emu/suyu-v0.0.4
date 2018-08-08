// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::Account {

struct UUID {
    // UUIDs which are 0 are considered invalid!
    u128 uuid{0, 0};
    UUID() = default;
    explicit UUID(const u128& id) {
        uuid[0] = id[0];
        uuid[1] = id[1];
    };
    explicit UUID(const u64& lo, const u64& hi) {
        uuid[0] = lo;
        uuid[1] = hi;
    };
    operator bool() const {
        return uuid[0] != 0x0 && uuid[1] != 0x0;
    }

    bool operator==(const UUID& rhs) {
        return uuid[0] == rhs.uuid[0] && uuid[1] == rhs.uuid[1];
    }

    bool operator!=(const UUID& rhs) {
        return uuid[0] != rhs.uuid[0] || uuid[1] != rhs.uuid[1];
    }

    // TODO(ogniK): Properly generate uuids based on RFC-4122
    const UUID& Generate() {
        uuid[0] = (static_cast<u64>(std::rand()) << 32) | std::rand();
        uuid[1] = (static_cast<u64>(std::rand()) << 32) | std::rand();
        return *this;
    }

    std::string Format() {
        return fmt::format("0x{:016X}{:016X}", uuid[1], uuid[0]);
    }
};
static_assert(sizeof(UUID) == 16, "UUID is an invalid size!");

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

    protected:
        std::shared_ptr<Module> module;
    };
};

/// Registers all ACC services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace Service::Account
