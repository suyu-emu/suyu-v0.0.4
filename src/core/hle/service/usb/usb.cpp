// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/usb/usb.h"

namespace Service::USB {

class IDsInterface final : public ServiceFramework<IDsInterface> {
public:
    explicit IDsInterface() : ServiceFramework{"IDsInterface"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetDsEndpoint"},
            {1, nullptr, "GetSetupEvent"},
            {2, nullptr, "Unknown"},
            {3, nullptr, "EnableInterface"},
            {4, nullptr, "DisableInterface"},
            {5, nullptr, "CtrlInPostBufferAsync"},
            {6, nullptr, "CtrlOutPostBufferAsync"},
            {7, nullptr, "GetCtrlInCompletionEvent"},
            {8, nullptr, "GetCtrlInReportData"},
            {9, nullptr, "GetCtrlOutCompletionEvent"},
            {10, nullptr, "GetCtrlOutReportData"},
            {11, nullptr, "StallCtrl"},
            {12, nullptr, "AppendConfigurationData"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class USB_DS final : public ServiceFramework<USB_DS> {
public:
    explicit USB_DS() : ServiceFramework{"usb:ds"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "BindDevice"},
            {1, nullptr, "BindClientProcess"},
            {2, nullptr, "GetDsInterface"},
            {3, nullptr, "GetStateChangeEvent"},
            {4, nullptr, "GetState"},
            {5, nullptr, "ClearDeviceData"},
            {6, nullptr, "AddUsbStringDescriptor"},
            {7, nullptr, "DeleteUsbStringDescriptor"},
            {8, nullptr, "SetUsbDeviceDescriptor"},
            {9, nullptr, "SetBinaryObjectStore"},
            {10, nullptr, "Enable"},
            {11, nullptr, "Disable"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IClientEpSession final : public ServiceFramework<IClientEpSession> {
public:
    explicit IClientEpSession() : ServiceFramework{"IClientEpSession"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown1"},
            {1, nullptr, "Unknown2"},
            {2, nullptr, "Unknown3"},
            {3, nullptr, "Unknown4"},
            {4, nullptr, "PostBufferAsync"},
            {5, nullptr, "Unknown5"},
            {6, nullptr, "Unknown6"},
            {7, nullptr, "Unknown7"},
            {8, nullptr, "Unknown8"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IClientIfSession final : public ServiceFramework<IClientIfSession> {
public:
    explicit IClientIfSession() : ServiceFramework{"IClientIfSession"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown1"},
            {1, nullptr, "Unknown2"},
            {2, nullptr, "Unknown3"},
            {3, nullptr, "Unknown4"},
            {4, nullptr, "Unknown5"},
            {5, nullptr, "CtrlXferAsync"},
            {6, nullptr, "Unknown6"},
            {7, nullptr, "GetCtrlXferReport"},
            {8, nullptr, "Unknown7"},
            {9, nullptr, "GetClientEpSession"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class USB_HS final : public ServiceFramework<USB_HS> {
public:
    explicit USB_HS() : ServiceFramework{"usb:hs"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "BindClientProcess"},
            {1, nullptr, "Unknown1"},
            {2, nullptr, "Unknown2"},
            {3, nullptr, "Unknown3"},
            {4, nullptr, "Unknown4"},
            {5, nullptr, "Unknown5"},
            {6, nullptr, "GetInterfaceStateChangeEvent"},
            {7, nullptr, "GetClientIfSession"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IPdSession final : public ServiceFramework<IPdSession> {
public:
    explicit IPdSession() : ServiceFramework{"IPdSession"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "BindNoticeEvent"},
            {1, nullptr, "Unknown1"},
            {2, nullptr, "GetStatus"},
            {3, nullptr, "GetNotice"},
            {4, nullptr, "Unknown2"},
            {5, nullptr, "Unknown3"},
            {6, nullptr, "ReplyPowerRequest"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class USB_PD final : public ServiceFramework<USB_PD> {
public:
    explicit USB_PD() : ServiceFramework{"usb:pd"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &USB_PD::GetPdSession, "GetPdSession"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetPdSession(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IPdSession>();

        LOG_DEBUG(Service_USB, "called");
    }
};

class IPdCradleSession final : public ServiceFramework<IPdCradleSession> {
public:
    explicit IPdCradleSession() : ServiceFramework{"IPdCradleSession"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "VdmUserWrite"},
            {1, nullptr, "VdmUserRead"},
            {2, nullptr, "Vdm20Init"},
            {3, nullptr, "GetFwType"},
            {4, nullptr, "GetFwRevision"},
            {5, nullptr, "GetManufacturerId"},
            {6, nullptr, "GetDeviceId"},
            {7, nullptr, "Unknown1"},
            {8, nullptr, "Unknown2"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class USB_PD_C final : public ServiceFramework<USB_PD_C> {
public:
    explicit USB_PD_C() : ServiceFramework{"usb:pd:c"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &USB_PD_C::GetPdCradleSession, "GetPdCradleSession"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetPdCradleSession(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IPdCradleSession>();

        LOG_DEBUG(Service_USB, "called");
    }
};

class USB_PM final : public ServiceFramework<USB_PM> {
public:
    explicit USB_PM() : ServiceFramework{"usb:pm"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown1"},
            {1, nullptr, "Unknown2"},
            {2, nullptr, "Unknown3"},
            {3, nullptr, "Unknown4"},
            {4, nullptr, "Unknown5"},
            {5, nullptr, "Unknown6"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<USB_DS>()->InstallAsService(sm);
    std::make_shared<USB_HS>()->InstallAsService(sm);
    std::make_shared<USB_PD>()->InstallAsService(sm);
    std::make_shared<USB_PD_C>()->InstallAsService(sm);
    std::make_shared<USB_PM>()->InstallAsService(sm);
}

} // namespace Service::USB
