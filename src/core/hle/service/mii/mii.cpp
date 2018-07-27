// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/service/mii/mii.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::Mii {

class IDatabaseService final : public ServiceFramework<IDatabaseService> {
public:
    explicit IDatabaseService() : ServiceFramework{"IDatabaseService"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "IsUpdated"},
            {1, nullptr, "IsFullDatabase"},
            {2, nullptr, "GetCount"},
            {3, nullptr, "Get"},
            {4, nullptr, "Get1"},
            {5, nullptr, "UpdateLatest"},
            {6, nullptr, "BuildRandom"},
            {7, nullptr, "BuildDefault"},
            {8, nullptr, "Get2"},
            {9, nullptr, "Get3"},
            {10, nullptr, "UpdateLatest1"},
            {11, nullptr, "FindIndex"},
            {12, nullptr, "Move"},
            {13, nullptr, "AddOrReplace"},
            {14, nullptr, "Delete"},
            {15, nullptr, "DestroyFile"},
            {16, nullptr, "DeleteFile"},
            {17, nullptr, "Format"},
            {18, nullptr, "Import"},
            {19, nullptr, "Export"},
            {20, nullptr, "IsBrokenDatabaseWithClearFlag"},
            {21, nullptr, "GetIndex"},
            {22, nullptr, "SetInterfaceVersion"},
            {23, nullptr, "Convert"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class MiiDBModule final : public ServiceFramework<MiiDBModule> {
public:
    explicit MiiDBModule(const char* name) : ServiceFramework{name} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &MiiDBModule::GetDatabaseService, "GetDatabaseService"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetDatabaseService(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IDatabaseService>();

        LOG_DEBUG(Service_Mii, "called");
    }
};

class MiiImg final : public ServiceFramework<MiiImg> {
public:
    explicit MiiImg() : ServiceFramework{"miiimg"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Initialize"},
            {10, nullptr, "Reload"},
            {11, nullptr, "GetCount"},
            {12, nullptr, "IsEmpty"},
            {13, nullptr, "IsFull"},
            {14, nullptr, "GetAttribute"},
            {15, nullptr, "LoadImage"},
            {16, nullptr, "AddOrUpdateImage"},
            {17, nullptr, "DeleteImages"},
            {100, nullptr, "DeleteFile"},
            {101, nullptr, "DestroyFile"},
            {102, nullptr, "ImportFile"},
            {103, nullptr, "ExportFile"},
            {104, nullptr, "ForceInitialize"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<MiiDBModule>("mii:e")->InstallAsService(sm);
    std::make_shared<MiiDBModule>("mii:u")->InstallAsService(sm);

    std::make_shared<MiiImg>()->InstallAsService(sm);
}

} // namespace Service::Mii
