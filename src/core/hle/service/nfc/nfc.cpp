// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::NFC {

class IAm final : public ServiceFramework<IAm> {
public:
    explicit IAm() : ServiceFramework{"IAm"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Initialize"},
            {1, nullptr, "Finalize"},
            {2, nullptr, "NotifyForegroundApplet"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class NFC_AM final : public ServiceFramework<NFC_AM> {
public:
    explicit NFC_AM() : ServiceFramework{"nfc:am"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "CreateAmInterface"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class MFIUser final : public ServiceFramework<MFIUser> {
public:
    explicit MFIUser() : ServiceFramework{"IUser"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Initialize"},
            {1, nullptr, "Finalize"},
            {2, nullptr, "ListDevices"},
            {3, nullptr, "StartDetection"},
            {4, nullptr, "StopDetection"},
            {5, nullptr, "Read"},
            {6, nullptr, "Write"},
            {7, nullptr, "GetTagInfo"},
            {8, nullptr, "GetActivateEventHandle"},
            {9, nullptr, "GetDeactivateEventHandle"},
            {10, nullptr, "GetState"},
            {11, nullptr, "GetDeviceState"},
            {12, nullptr, "GetNpadId"},
            {13, nullptr, "GetAvailabilityChangeEventHandle"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class NFC_MF_U final : public ServiceFramework<NFC_MF_U> {
public:
    explicit NFC_MF_U() : ServiceFramework{"nfc:mf:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "CreateUserInterface"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IUser final : public ServiceFramework<IUser> {
public:
    explicit IUser() : ServiceFramework{"IUser"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Initialize"},
            {1, nullptr, "Finalize"},
            {2, nullptr, "GetState"},
            {3, nullptr, "IsNfcEnabled"},
            {400, nullptr, "Initialize"},
            {401, nullptr, "Finalize"},
            {402, nullptr, "GetState"},
            {403, nullptr, "IsNfcEnabled"},
            {404, nullptr, "ListDevices"},
            {405, nullptr, "GetDeviceState"},
            {406, nullptr, "GetNpadId"},
            {407, nullptr, "AttachAvailabilityChangeEvent"},
            {408, nullptr, "StartDetection"},
            {409, nullptr, "StopDetection"},
            {410, nullptr, "GetTagInfo"},
            {411, nullptr, "AttachActivateEvent"},
            {412, nullptr, "AttachDeactivateEvent"},
            {1000, nullptr, "ReadMifare"},
            {1001, nullptr, "WriteMifare"},
            {1300, nullptr, "SendCommandByPassThrough"},
            {1301, nullptr, "KeepPassThroughSession"},
            {1302, nullptr, "ReleasePassThroughSession"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class NFC_U final : public ServiceFramework<NFC_U> {
public:
    explicit NFC_U() : ServiceFramework{"nfc:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "CreateUserInterface"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class ISystem final : public ServiceFramework<ISystem> {
public:
    explicit ISystem() : ServiceFramework{"ISystem"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Initialize"},
            {1, nullptr, "Finalize"},
            {2, nullptr, "GetState"},
            {3, nullptr, "IsNfcEnabled"},
            {100, nullptr, "SetNfcEnabled"},
            {400, nullptr, "InitializeSystem"},
            {401, nullptr, "FinalizeSystem"},
            {402, nullptr, "GetState"},
            {403, nullptr, "IsNfcEnabled"},
            {404, nullptr, "ListDevices"},
            {405, nullptr, "GetDeviceState"},
            {406, nullptr, "GetNpadId"},
            {407, nullptr, "AttachAvailabilityChangeEvent"},
            {408, nullptr, "StartDetection"},
            {409, nullptr, "StopDetection"},
            {410, nullptr, "GetTagInfo"},
            {411, nullptr, "AttachActivateEvent"},
            {412, nullptr, "AttachDeactivateEvent"},
            {500, nullptr, "SetNfcEnabled"},
            {1000, nullptr, "ReadMifare"},
            {1001, nullptr, "WriteMifare"},
            {1300, nullptr, "SendCommandByPassThrough"},
            {1301, nullptr, "KeepPassThroughSession"},
            {1302, nullptr, "ReleasePassThroughSession"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class NFC_SYS final : public ServiceFramework<NFC_SYS> {
public:
    explicit NFC_SYS() : ServiceFramework{"nfc:sys"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "CreateSystemInterface"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<NFC_AM>()->InstallAsService(sm);
    std::make_shared<NFC_MF_U>()->InstallAsService(sm);
    std::make_shared<NFC_U>()->InstallAsService(sm);
    std::make_shared<NFC_SYS>()->InstallAsService(sm);
}

} // namespace Service::NFC
