// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "core/hle/service/caps/caps.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::Capture {

class CAPS_A final : public ServiceFramework<CAPS_A> {
public:
    explicit CAPS_A() : ServiceFramework{"caps:a"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown1"},
            {1, nullptr, "Unknown2"},
            {2, nullptr, "Unknown3"},
            {3, nullptr, "Unknown4"},
            {4, nullptr, "Unknown5"},
            {5, nullptr, "Unknown6"},
            {6, nullptr, "Unknown7"},
            {7, nullptr, "Unknown8"},
            {8, nullptr, "Unknown9"},
            {9, nullptr, "Unknown10"},
            {10, nullptr, "Unknown11"},
            {11, nullptr, "Unknown12"},
            {12, nullptr, "Unknown13"},
            {13, nullptr, "Unknown14"},
            {14, nullptr, "Unknown15"},
            {301, nullptr, "Unknown16"},
            {401, nullptr, "Unknown17"},
            {501, nullptr, "Unknown18"},
            {1001, nullptr, "Unknown19"},
            {1002, nullptr, "Unknown20"},
            {8001, nullptr, "Unknown21"},
            {8002, nullptr, "Unknown22"},
            {8011, nullptr, "Unknown23"},
            {8012, nullptr, "Unknown24"},
            {8021, nullptr, "Unknown25"},
            {10011, nullptr, "Unknown26"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class CAPS_C final : public ServiceFramework<CAPS_C> {
public:
    explicit CAPS_C() : ServiceFramework{"caps:c"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {2001, nullptr, "Unknown1"},
            {2002, nullptr, "Unknown2"},
            {2011, nullptr, "Unknown3"},
            {2012, nullptr, "Unknown4"},
            {2013, nullptr, "Unknown5"},
            {2014, nullptr, "Unknown6"},
            {2101, nullptr, "Unknown7"},
            {2102, nullptr, "Unknown8"},
            {2201, nullptr, "Unknown9"},
            {2301, nullptr, "Unknown10"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class CAPS_SC final : public ServiceFramework<CAPS_SC> {
public:
    explicit CAPS_SC() : ServiceFramework{"caps:sc"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, nullptr, "Unknown1"},
            {2, nullptr, "Unknown2"},
            {1001, nullptr, "Unknown3"},
            {1002, nullptr, "Unknown4"},
            {1003, nullptr, "Unknown5"},
            {1011, nullptr, "Unknown6"},
            {1012, nullptr, "Unknown7"},
            {1201, nullptr, "Unknown8"},
            {1202, nullptr, "Unknown9"},
            {1203, nullptr, "Unknown10"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class CAPS_SS final : public ServiceFramework<CAPS_SS> {
public:
    explicit CAPS_SS() : ServiceFramework{"caps:ss"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {201, nullptr, "Unknown1"},
            {202, nullptr, "Unknown2"},
            {203, nullptr, "Unknown3"},
            {204, nullptr, "Unknown4"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class CAPS_SU final : public ServiceFramework<CAPS_SU> {
public:
    explicit CAPS_SU() : ServiceFramework{"caps:su"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {201, nullptr, "SaveScreenShot"},
            {203, nullptr, "SaveScreenShotEx0"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class CAPS_U final : public ServiceFramework<CAPS_U> {
public:
    explicit CAPS_U() : ServiceFramework{"caps:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {102, nullptr, "GetAlbumFileListByAruid"},
            {103, nullptr, "DeleteAlbumFileByAruid"},
            {104, nullptr, "GetAlbumFileSizeByAruid"},
            {110, nullptr, "LoadAlbumScreenShotImageByAruid"},
            {120, nullptr, "LoadAlbumScreenShotThumbnailImageByAruid"},
            {60002, nullptr, "OpenAccessorSessionForApplication"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<CAPS_A>()->InstallAsService(sm);
    std::make_shared<CAPS_C>()->InstallAsService(sm);
    std::make_shared<CAPS_SC>()->InstallAsService(sm);
    std::make_shared<CAPS_SS>()->InstallAsService(sm);
    std::make_shared<CAPS_SU>()->InstallAsService(sm);
    std::make_shared<CAPS_U>()->InstallAsService(sm);
}

} // namespace Service::Capture
