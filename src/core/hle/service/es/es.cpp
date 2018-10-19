// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/service.h"

namespace Service::ES {

class ETicket final : public ServiceFramework<ETicket> {
public:
    explicit ETicket() : ServiceFramework{"es"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, nullptr, "ImportTicket"},
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
};

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<ETicket>()->InstallAsService(service_manager);
}

} // namespace Service::ES
