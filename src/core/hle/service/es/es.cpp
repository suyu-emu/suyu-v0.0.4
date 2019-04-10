// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/crypto/key_manager.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/service.h"

namespace Service::ES {

constexpr ResultCode ERROR_INVALID_ARGUMENT{ErrorModule::ETicket, 2};
constexpr ResultCode ERROR_INVALID_RIGHTS_ID{ErrorModule::ETicket, 3};

class ETicket final : public ServiceFramework<ETicket> {
public:
    explicit ETicket() : ServiceFramework{"es"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, &ETicket::ImportTicket, "ImportTicket"},
            {2, nullptr, "ImportTicketCertificateSet"},
            {3, nullptr, "DeleteTicket"},
            {4, nullptr, "DeletePersonalizedTicket"},
            {5, nullptr, "DeleteAllCommonTicket"},
            {6, nullptr, "DeleteAllPersonalizedTicket"},
            {7, nullptr, "DeleteAllPersonalizedTicketEx"},
            {8, nullptr, "GetTitleKey"},
            {9, nullptr, "CountCommonTicket"},
            {10, nullptr, "CountPersonalizedTicket"},
            {11, nullptr, "ListCommonTicket"},
            {12, nullptr, "ListPersonalizedTicket"},
            {13, nullptr, "ListMissingPersonalizedTicket"},
            {14, nullptr, "GetCommonTicketSize"},
            {15, nullptr, "GetPersonalizedTicketSize"},
            {16, nullptr, "GetCommonTicketData"},
            {17, nullptr, "GetPersonalizedTicketData"},
            {18, nullptr, "OwnTicket"},
            {19, nullptr, "GetTicketInfo"},
            {20, nullptr, "ListLightTicketInfo"},
            {21, nullptr, "SignData"},
            {22, nullptr, "GetCommonTicketAndCertificateSize"},
            {23, nullptr, "GetCommonTicketAndCertificateData"},
            {24, nullptr, "ImportPrepurchaseRecord"},
            {25, nullptr, "DeletePrepurchaseRecord"},
            {26, nullptr, "DeleteAllPrepurchaseRecord"},
            {27, nullptr, "CountPrepurchaseRecord"},
            {28, nullptr, "ListPrepurchaseRecordRightsIds"},
            {29, nullptr, "ListPrepurchaseRecordInfo"},
            {30, nullptr, "CountTicket"},
            {31, nullptr, "ListTicketRightsIds"},
            {32, nullptr, "CountPrepurchaseRecordEx"},
            {33, nullptr, "ListPrepurchaseRecordRightsIdsEx"},
            {34, nullptr, "GetEncryptedTicketSize"},
            {35, nullptr, "GetEncryptedTicketData"},
            {36, nullptr, "DeleteAllInactiveELicenseRequiredPersonalizedTicket"},
            {503, nullptr, "GetTitleKey"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }

private:
    bool CheckRightsId(Kernel::HLERequestContext& ctx, const u128& rights_id) {
        if (rights_id == u128{}) {
            LOG_ERROR(Service_ETicket, "The rights ID was invalid!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_INVALID_RIGHTS_ID);
            return false;
        }

        return true;
    }

    void ImportTicket(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto ticket = ctx.ReadBuffer();
        const auto cert = ctx.ReadBuffer(1);

        if (ticket.size() < sizeof(Core::Crypto::TicketRaw)) {
            LOG_ERROR(Service_ETicket, "The input buffer is not large enough!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_INVALID_ARGUMENT);
            return;
        }

        Core::Crypto::TicketRaw raw;
        std::memcpy(raw.data(), ticket.data(), sizeof(Core::Crypto::TicketRaw));

        if (!keys.AddTicketPersonalized(raw)) {
            LOG_ERROR(Service_ETicket, "The ticket could not be imported!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_INVALID_ARGUMENT);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

};

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<ETicket>()->InstallAsService(service_manager);
}

} // namespace Service::ES
