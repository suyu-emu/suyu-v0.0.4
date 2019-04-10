// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "core/hle/service/ncm/ncm.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::NCM {

class LocationResolver final : public ServiceFramework<LocationResolver> {
class ILocationResolver final : public ServiceFramework<ILocationResolver> {
public:
    explicit ILocationResolver(FileSys::StorageId id)
        : ServiceFramework{"ILocationResolver"}, storage(id) {
        static const FunctionInfo functions[] = {
            {0, nullptr, "ResolveProgramPath"},
            {1, nullptr, "RedirectProgramPath"},
            {2, nullptr, "ResolveApplicationControlPath"},
            {3, nullptr, "ResolveApplicationHtmlDocumentPath"},
            {4, nullptr, "ResolveDataPath"},
            {5, nullptr, "RedirectApplicationControlPath"},
            {6, nullptr, "RedirectApplicationHtmlDocumentPath"},
            {7, nullptr, "ResolveApplicationLegalInformationPath"},
            {8, nullptr, "RedirectApplicationLegalInformationPath"},
            {9, nullptr, "Refresh"},
            {10, nullptr, "RedirectProgramPath2"},
            {11, nullptr, "Refresh2"},
            {12, nullptr, "DeleteProgramPath"},
            {13, nullptr, "DeleteApplicationControlPath"},
            {14, nullptr, "DeleteApplicationHtmlDocumentPath"},
            {15, nullptr, "DeleteApplicationLegalInformationPath"},
            {16, nullptr, ""},
            {17, nullptr, ""},
            {18, nullptr, ""},
            {19, nullptr, ""},
        };

        RegisterHandlers(functions);
    }

private:
    FileSys::StorageId storage;
};

public:
    explicit LocationResolver() : ServiceFramework{"lr"} {
class LR final : public ServiceFramework<LR> {
public:
    explicit LR() : ServiceFramework{"lr"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "OpenLocationResolver"},
            {1, nullptr, "OpenRegisteredLocationResolver"},
            {2, nullptr, "RefreshLocationResolver"},
            {3, nullptr, "OpenAddOnContentLocationResolver"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void OpenLocationResolver(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto id = rp.PopRaw<FileSys::StorageId>();

        LOG_DEBUG(Service_NCM, "called, id={:02X}", static_cast<u8>(id));

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface(std::make_shared<ILocationResolver>(id));
    }

};

class NCM final : public ServiceFramework<NCM> {
public:
    explicit NCM() : ServiceFramework{"ncm"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "CreateContentStorage"},
            {1, nullptr, "CreateContentMetaDatabase"},
            {2, nullptr, "VerifyContentStorage"},
            {3, nullptr, "VerifyContentMetaDatabase"},
            {4, nullptr, "OpenContentStorage"},
            {5, nullptr, "OpenContentMetaDatabase"},
            {6, nullptr, "CloseContentStorageForcibly"},
            {7, nullptr, "CloseContentMetaDatabaseForcibly"},
            {8, nullptr, "CleanupContentMetaDatabase"},
            {9, nullptr, "ActivateContentStorage"},
            {10, nullptr, "InactivateContentStorage"},
            {11, nullptr, "ActivateContentMetaDatabase"},
            {12, nullptr, "InactivateContentMetaDatabase"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<LocationResolver>()->InstallAsService(sm);
    std::make_shared<NCM>()->InstallAsService(sm);
}

} // namespace Service::NCM
