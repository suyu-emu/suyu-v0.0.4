// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/friend/friend.h"
#include "core/hle/service/friend/interface.h"

namespace Service::Friend {

class IFriendService final : public ServiceFramework<IFriendService> {
public:
    IFriendService() : ServiceFramework("IFriendService") {
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetCompletionEvent"},
            {1, nullptr, "Cancel"},
            {10100, nullptr, "GetFriendListIds"},
            {10101, nullptr, "GetFriendList"},
            {10102, nullptr, "UpdateFriendInfo"},
            {10110, nullptr, "GetFriendProfileImage"},
            {10200, nullptr, "SendFriendRequestForApplication"},
            {10211, nullptr, "AddFacedFriendRequestForApplication"},
            {10400, nullptr, "GetBlockedUserListIds"},
            {10500, nullptr, "GetProfileList"},
            {10600, nullptr, "DeclareOpenOnlinePlaySession"},
            {10601, &IFriendService::DeclareCloseOnlinePlaySession,
             "DeclareCloseOnlinePlaySession"},
            {10610, nullptr, "UpdateUserPresence"},
            {10700, nullptr, "GetPlayHistoryRegistrationKey"},
            {10701, nullptr, "GetPlayHistoryRegistrationKeyWithNetworkServiceAccountId"},
            {10702, nullptr, "AddPlayHistory"},
            {11000, nullptr, "GetProfileImageUrl"},
            {20100, nullptr, "GetFriendCount"},
            {20101, nullptr, "GetNewlyFriendCount"},
            {20102, nullptr, "GetFriendDetailedInfo"},
            {20103, nullptr, "SyncFriendList"},
            {20104, nullptr, "RequestSyncFriendList"},
            {20110, nullptr, "LoadFriendSetting"},
            {20200, nullptr, "GetReceivedFriendRequestCount"},
            {20201, nullptr, "GetFriendRequestList"},
            {20300, nullptr, "GetFriendCandidateList"},
            {20301, nullptr, "GetNintendoNetworkIdInfo"},
            {20302, nullptr, "GetSnsAccountLinkage"},
            {20303, nullptr, "GetSnsAccountProfile"},
            {20304, nullptr, "GetSnsAccountFriendList"},
            {20400, nullptr, "GetBlockedUserList"},
            {20401, nullptr, "SyncBlockedUserList"},
            {20500, nullptr, "GetProfileExtraList"},
            {20501, nullptr, "GetRelationship"},
            {20600, nullptr, "GetUserPresenceView"},
            {20700, nullptr, "GetPlayHistoryList"},
            {20701, nullptr, "GetPlayHistoryStatistics"},
            {20800, nullptr, "LoadUserSetting"},
            {20801, nullptr, "SyncUserSetting"},
            {20900, nullptr, "RequestListSummaryOverlayNotification"},
            {21000, nullptr, "GetExternalApplicationCatalog"},
            {30100, nullptr, "DropFriendNewlyFlags"},
            {30101, nullptr, "DeleteFriend"},
            {30110, nullptr, "DropFriendNewlyFlag"},
            {30120, nullptr, "ChangeFriendFavoriteFlag"},
            {30121, nullptr, "ChangeFriendOnlineNotificationFlag"},
            {30200, nullptr, "SendFriendRequest"},
            {30201, nullptr, "SendFriendRequestWithApplicationInfo"},
            {30202, nullptr, "CancelFriendRequest"},
            {30203, nullptr, "AcceptFriendRequest"},
            {30204, nullptr, "RejectFriendRequest"},
            {30205, nullptr, "ReadFriendRequest"},
            {30210, nullptr, "GetFacedFriendRequestRegistrationKey"},
            {30211, nullptr, "AddFacedFriendRequest"},
            {30212, nullptr, "CancelFacedFriendRequest"},
            {30213, nullptr, "GetFacedFriendRequestProfileImage"},
            {30214, nullptr, "GetFacedFriendRequestProfileImageFromPath"},
            {30215, nullptr, "SendFriendRequestWithExternalApplicationCatalogId"},
            {30216, nullptr, "ResendFacedFriendRequest"},
            {30217, nullptr, "SendFriendRequestWithNintendoNetworkIdInfo"},
            {30300, nullptr, "GetSnsAccountLinkPageUrl"},
            {30301, nullptr, "UnlinkSnsAccount"},
            {30400, nullptr, "BlockUser"},
            {30401, nullptr, "BlockUserWithApplicationInfo"},
            {30402, nullptr, "UnblockUser"},
            {30500, nullptr, "GetProfileExtraFromFriendCode"},
            {30700, nullptr, "DeletePlayHistory"},
            {30810, nullptr, "ChangePresencePermission"},
            {30811, nullptr, "ChangeFriendRequestReception"},
            {30812, nullptr, "ChangePlayLogPermission"},
            {30820, nullptr, "IssueFriendCode"},
            {30830, nullptr, "ClearPlayLog"},
            {49900, nullptr, "DeleteNetworkServiceAccountCache"},
        };

        RegisterHandlers(functions);
    }

private:
    void DeclareCloseOnlinePlaySession(Kernel::HLERequestContext& ctx) {
        // Stub used by Splatoon 2
        LOG_WARNING(Service_ACC, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }
};

void Module::Interface::CreateFriendService(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IFriendService>();
    LOG_DEBUG(Service_ACC, "called");
}

Module::Interface::Interface(std::shared_ptr<Module> module, const char* name)
    : ServiceFramework(name), module(std::move(module)) {}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto module = std::make_shared<Module>();
    std::make_shared<Friend>(module, "friend:a")->InstallAsService(service_manager);
    std::make_shared<Friend>(module, "friend:m")->InstallAsService(service_manager);
    std::make_shared<Friend>(module, "friend:s")->InstallAsService(service_manager);
    std::make_shared<Friend>(module, "friend:u")->InstallAsService(service_manager);
    std::make_shared<Friend>(module, "friend:v")->InstallAsService(service_manager);
}

} // namespace Service::Friend
