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
            {0, nullptr, "GetAlbumFileCount"},
            {1, nullptr, "GetAlbumFileList"},
            {2, nullptr, "LoadAlbumFile"},
            {3, nullptr, "DeleteAlbumFile"},
            {4, nullptr, "StorageCopyAlbumFile"},
            {5, nullptr, "IsAlbumMounted"},
            {6, nullptr, "GetAlbumUsage"},
            {7, nullptr, "GetAlbumFileSize"},
            {8, nullptr, "LoadAlbumFileThumbnail"},
            {9, nullptr, "LoadAlbumScreenShotImage"},
            {10, nullptr, "LoadAlbumScreenShotThumbnailImage"},
            {11, nullptr, "GetAlbumEntryFromApplicationAlbumEntry"},
            {12, nullptr, "Unknown12"},
            {13, nullptr, "Unknown13"},
            {14, nullptr, "Unknown14"},
            {15, nullptr, "Unknown15"},
            {16, nullptr, "Unknown16"},
            {17, nullptr, "Unknown17"},
            {18, nullptr, "Unknown18"},
            {202, nullptr, "SaveEditedScreenShot"},
            {301, nullptr, "GetLastThumbnail"},
            {401, nullptr, "GetAutoSavingStorage"},
            {501, nullptr, "GetRequiredStorageSpaceSizeToCopyAll"},
            {1001, nullptr, "Unknown1001"},
            {1002, nullptr, "Unknown1002"},
            {1003, nullptr, "Unknown1003"},
            {8001, nullptr, "ForceAlbumUnmounted"},
            {8002, nullptr, "ResetAlbumMountStatus"},
            {8011, nullptr, "RefreshAlbumCache"},
            {8012, nullptr, "GetAlbumCache"},
            {8013, nullptr, "Unknown8013"},
            {8021, nullptr, "GetAlbumEntryFromApplicationAlbumEntryAruid"},
            {10011, nullptr, "SetInternalErrorConversionEnabled"},
            {50000, nullptr, "Unknown50000"},
            {60002, nullptr, "Unknown60002"},
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
            {33, nullptr, "Unknown33"},
            {2001, nullptr, "Unknown2001"},
            {2002, nullptr, "Unknown2002"},
            {2011, nullptr, "Unknown2011"},
            {2012, nullptr, "Unknown2012"},
            {2013, nullptr, "Unknown2013"},
            {2014, nullptr, "Unknown2014"},
            {2101, nullptr, "Unknown2101"},
            {2102, nullptr, "Unknown2102"},
            {2201, nullptr, "Unknown2201"},
            {2301, nullptr, "Unknown2301"},
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
            {32, nullptr, "SetShimLibraryVersion"},
            {102, nullptr, "GetAlbumFileListByAruid"},
            {103, nullptr, "DeleteAlbumFileByAruid"},
            {104, nullptr, "GetAlbumFileSizeByAruid"},
            {105, nullptr, "DeleteAlbumFileByAruidForDebug"},
            {110, nullptr, "LoadAlbumScreenShotImageByAruid"},
            {120, nullptr, "LoadAlbumScreenShotThumbnailImageByAruid"},
            {130, nullptr, "PrecheckToCreateContentsByAruid"},
            {140, nullptr, "GetAlbumFileList1AafeAruidDeprecated"},
            {141, nullptr, "GetAlbumFileList2AafeUidAruidDeprecated"},
            {142, nullptr, "GetAlbumFileList3AaeAruid"},
            {143, nullptr, "GetAlbumFileList4AaeUidAruid"},
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
