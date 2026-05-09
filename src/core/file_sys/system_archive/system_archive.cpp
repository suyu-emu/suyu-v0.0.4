// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/system_archive/mii_model.h"
#include "core/file_sys/system_archive/ng_word.h"
#include "core/file_sys/system_archive/shared_font.h"
#include "core/file_sys/system_archive/system_archive.h"
#include "core/file_sys/system_archive/system_version.h"
#include "core/file_sys/system_archive/time_zone_binary.h"

namespace FileSys::SystemArchive {

struct SystemArchiveDescriptor {
    const char* name;
    VirtualDir (*supplier)();
};

constexpr inline SystemArchiveDescriptor GetSystemArchive(u64 title_id) {
    switch (title_id) {
    case 0x0100000000000800: return {"CertStore", nullptr};
    case 0x0100000000000801: return {"ErrorMessage", nullptr};
    case 0x0100000000000802: return {"MiiModel", &MiiModel};
    case 0x0100000000000803: return {"BrowserDll", nullptr};
    case 0x0100000000000804: return {"Help", nullptr};
    case 0x0100000000000805: return {"SharedFont", nullptr};
    case 0x0100000000000806: return {"NgWord", &NgWord1};
    case 0x0100000000000807: return {"SsidList", nullptr};
    case 0x0100000000000808: return {"Dictionary", nullptr};
    case 0x0100000000000809: return {"SystemVersion", &SystemVersion};
    case 0x010000000000080A: return {"AvatarImage", nullptr};
    case 0x010000000000080B: return {"LocalNews", nullptr};
    case 0x010000000000080C: return {"Eula", nullptr};
    case 0x010000000000080D: return {"UrlBlackList", nullptr};
    case 0x010000000000080E: return {"TimeZoneBinary", &TimeZoneBinary};
    case 0x010000000000080F: return {"CertStoreCruiser", nullptr};
    case 0x0100000000000810: return {"FontNintendoExtension", &FontNintendoExtension};
    case 0x0100000000000811: return {"FontStandard", &FontStandard};
    case 0x0100000000000812: return {"FontKorean", &FontKorean};
    case 0x0100000000000813: return {"FontChineseTraditional", &FontChineseTraditional};
    case 0x0100000000000814: return {"FontChineseSimple", &FontChineseSimple};
    case 0x0100000000000815: return {"FontBfcpx", nullptr};
    case 0x0100000000000816: return {"SystemUpdate", nullptr};
    case 0x0100000000000817: return {"0100000000000817", nullptr};
    case 0x0100000000000818: return {"FirmwareDebugSettings", nullptr};
    case 0x0100000000000819: return {"BootImagePackage", nullptr};
    case 0x010000000000081A: return {"BootImagePackageSafe", nullptr};
    case 0x010000000000081B: return {"BootImagePackageExFat", nullptr};
    case 0x010000000000081C: return {"BootImagePackageExFatSafe", nullptr};
    case 0x010000000000081D: return {"FatalMessage", nullptr};
    case 0x010000000000081E: return {"ControllerIcon", nullptr};
    case 0x010000000000081F: return {"PlatformConfigIcosa", nullptr};
    case 0x0100000000000820: return {"PlatformConfigCopper", nullptr};
    case 0x0100000000000821: return {"PlatformConfigHoag", nullptr};
    case 0x0100000000000822: return {"ControllerFirmware", nullptr};
    case 0x0100000000000823: return {"NgWord2", &NgWord2};
    case 0x0100000000000824: return {"PlatformConfigIcosaMariko", nullptr};
    case 0x0100000000000825: return {"ApplicationBlackList", nullptr};
    case 0x0100000000000826: return {"RebootlessSystemUpdateVersion", nullptr};
    case 0x0100000000000827: return {"ContentActionTable", nullptr};
    case 0x0100000000000828: return {"FunctionBlackList", nullptr};
    case 0x0100000000000829: return {"PlatformConfigCalcio", nullptr};
    case 0x0100000000000830: return {"NgWordT", nullptr};
    case 0x0100000000000831: return {"PlatformConfigAula", nullptr};
    case 0x0100000000000832: return {"CradleFirmware", nullptr};
    case 0x0100000000000835: return {"ErrorMessageUtf8", nullptr};
    case 0x0100000000000859: return {"ClientCertData", nullptr};
    case 0x010000000000085C: return {"GameCardConfigurationData", nullptr};
    default: return {nullptr, nullptr};
    }
}

VirtualFile SynthesizeSystemArchive(const u64 title_id) {
    auto const desc = GetSystemArchive(title_id);
    LOG_INFO(Service_FS, "Synthesizing system archive '{}' (0x{:016X}).", desc.name, title_id);
    if (desc.supplier != nullptr) {
        if (auto const dir = desc.supplier(); dir != nullptr) {
            if (auto const romfs = CreateRomFS(dir); romfs != nullptr) {
                LOG_INFO(Service_FS, "    - System archive generation successful!");
                return romfs;
            }
        }
    }
    return nullptr;
}
} // namespace FileSys::SystemArchive
