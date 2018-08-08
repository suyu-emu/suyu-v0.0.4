// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/swap.h"
#include "core/core_timing.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/acc/acc.h"
#include "core/hle/service/acc/acc_aa.h"
#include "core/hle/service/acc/acc_su.h"
#include "core/hle/service/acc/acc_u0.h"
#include "core/hle/service/acc/acc_u1.h"
#include "core/settings.h"

namespace Service::Account {
// TODO: RE this structure
struct UserData {
    INSERT_PADDING_WORDS(1);
    u32 icon_id;
    u8 bg_color_id;
    INSERT_PADDING_BYTES(0x7);
    INSERT_PADDING_BYTES(0x10);
    INSERT_PADDING_BYTES(0x60);
};
static_assert(sizeof(UserData) == 0x80, "UserData structure has incorrect size");

// TODO(ogniK): Generate a real user id based on username, md5(username) maybe?
static UUID DEFAULT_USER_ID{1ull, 0ull};

class IProfile final : public ServiceFramework<IProfile> {
public:
    explicit IProfile(UUID user_id, ProfileManager& profile_manager)
        : ServiceFramework("IProfile"), user_id(user_id), profile_manager(profile_manager) {
        static const FunctionInfo functions[] = {
            {0, &IProfile::Get, "Get"},
            {1, &IProfile::GetBase, "GetBase"},
            {10, nullptr, "GetImageSize"},
            {11, nullptr, "LoadImage"},
        };
        RegisterHandlers(functions);
    }

private:
    void Get(Kernel::HLERequestContext& ctx) {
        LOG_INFO(Service_ACC, "called user_id={}", user_id.Format());
        ProfileBase profile_base{};
        std::array<u8, MAX_DATA> data{};
        /*if (Settings::values.username.size() > profile_base.username.size()) {
            std::copy_n(Settings::values.username.begin(), profile_base.username.size(),
                        profile_base.username.begin());
        } else {
            std::copy(Settings::values.username.begin(), Settings::values.username.end(),
                      profile_base.username.begin());
        }*/
        if (profile_manager.GetProfileBaseAndData(user_id, profile_base, data)) {
            ctx.WriteBuffer(data);
            IPC::ResponseBuilder rb{ctx, 16};
            rb.Push(RESULT_SUCCESS);
            rb.PushRaw(profile_base);
        } else {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ResultCode(-1)); // TODO(ogniK): Get actual error code
        }
    }

    void GetBase(Kernel::HLERequestContext& ctx) {
        LOG_INFO(Service_ACC, "called user_id={}", user_id.Format());
        ProfileBase profile_base{};
        if (profile_manager.GetProfileBase(user_id, profile_base)) {
            IPC::ResponseBuilder rb{ctx, 16};
            rb.Push(RESULT_SUCCESS);
            rb.PushRaw(profile_base);
        } else {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ResultCode(-1)); // TODO(ogniK): Get actual error code
        }
    }

    ProfileManager& profile_manager;
    UUID user_id; ///< The user id this profile refers to.
};

class IManagerForApplication final : public ServiceFramework<IManagerForApplication> {
public:
    IManagerForApplication() : ServiceFramework("IManagerForApplication") {
        static const FunctionInfo functions[] = {
            {0, &IManagerForApplication::CheckAvailability, "CheckAvailability"},
            {1, &IManagerForApplication::GetAccountId, "GetAccountId"},
            {2, nullptr, "EnsureIdTokenCacheAsync"},
            {3, nullptr, "LoadIdTokenCache"},
            {130, nullptr, "GetNintendoAccountUserResourceCacheForApplication"},
            {150, nullptr, "CreateAuthorizationRequest"},
            {160, nullptr, "StoreOpenContext"},
        };
        RegisterHandlers(functions);
    }

private:
    void CheckAvailability(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_ACC, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push(false); // TODO: Check when this is supposed to return true and when not
    }

    void GetAccountId(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_ACC, "(STUBBED) called");
        // TODO(Subv): Find out what this actually does and implement it. Stub it as an error for
        // now since we do not implement NNID. Returning a bogus id here will cause games to send
        // invalid IPC requests after ListOpenUsers is called.
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultCode(-1));
    }
};

void Module::Interface::GetUserCount(Kernel::HLERequestContext& ctx) {
    LOG_INFO(Service_ACC, "called");
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(static_cast<u32>(profile_manager->GetUserCount()));
}

void Module::Interface::GetUserExistence(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    UUID user_id = rp.PopRaw<UUID>();
    LOG_INFO(Service_ACC, "called user_id={}", user_id.Format());

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.Push(profile_manager->UserExists(user_id));
}

void Module::Interface::ListAllUsers(Kernel::HLERequestContext& ctx) {
    LOG_INFO(Service_ACC, "called");
    ctx.WriteBuffer(profile_manager->GetAllUsers());
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::ListOpenUsers(Kernel::HLERequestContext& ctx) {
    LOG_INFO(Service_ACC, "called");
    ctx.WriteBuffer(profile_manager->GetOpenUsers());
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::GetLastOpenedUser(Kernel::HLERequestContext& ctx) {
    LOG_INFO(Service_ACC, "called");
    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<UUID>(profile_manager->GetLastOpennedUser());
}

void Module::Interface::GetProfile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    UUID user_id = rp.PopRaw<UUID>();
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IProfile>(user_id, *profile_manager);
    LOG_DEBUG(Service_ACC, "called user_id={}", user_id.Format());
}

void Module::Interface::InitializeApplicationInfo(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_ACC, "(STUBBED) called");
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::GetBaasAccountManagerForApplication(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IManagerForApplication>();
    LOG_DEBUG(Service_ACC, "called");
}

Module::Interface::Interface(std::shared_ptr<Module> module, const char* name)
    : ServiceFramework(name), module(std::move(module)) {}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto module = std::make_shared<Module>();
    std::make_shared<ACC_AA>(module)->InstallAsService(service_manager);
    std::make_shared<ACC_SU>(module)->InstallAsService(service_manager);
    std::make_shared<ACC_U0>(module)->InstallAsService(service_manager);
    std::make_shared<ACC_U1>(module)->InstallAsService(service_manager);
}

} // namespace Service::Account
