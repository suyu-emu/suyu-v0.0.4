// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstring>
#include <vector>

#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/hex_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/mode.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/system_archive/system_archive.h"
#include "core/file_sys/vfs_types.h"
#include "core/frontend/applets/general_frontend.h"
#include "core/frontend/applets/web_browser.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/am/applets/web_browser.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/loader.h"

namespace Service::AM::Applets {

enum class WebArgTLVType : u16 {
    InitialURL = 0x1,
    ShopArgumentsURL = 0x2, ///< TODO(DarkLordZach): This is not the official name.
    CallbackURL = 0x3,
    CallbackableURL = 0x4,
    ApplicationID = 0x5,
    DocumentPath = 0x6,
    DocumentKind = 0x7,
    SystemDataID = 0x8,
    ShareStartPage = 0x9,
    Whitelist = 0xA,
    News = 0xB,
    UserID = 0xE,
    AlbumEntry0 = 0xF,
    ScreenShotEnabled = 0x10,
    EcClientCertEnabled = 0x11,
    Unk12 = 0x12,
    PlayReportEnabled = 0x13,
    Unk14 = 0x14,
    Unk15 = 0x15,
    BootDisplayKind = 0x17,
    BackgroundKind = 0x18,
    FooterEnabled = 0x19,
    PointerEnabled = 0x1A,
    LeftStickMode = 0x1B,
    KeyRepeatFrame1 = 0x1C,
    KeyRepeatFrame2 = 0x1D,
    BootAsMediaPlayerInv = 0x1E,
    DisplayUrlKind = 0x1F,
    BootAsMediaPlayer = 0x21,
    ShopJumpEnabled = 0x22,
    MediaAutoPlayEnabled = 0x23,
    LobbyParameter = 0x24,
    ApplicationAlbumEntry = 0x26,
    JsExtensionEnabled = 0x27,
    AdditionalCommentText = 0x28,
    TouchEnabledOnContents = 0x29,
    UserAgentAdditionalString = 0x2A,
    AdditionalMediaData0 = 0x2B,
    MediaPlayerAutoCloseEnabled = 0x2C,
    PageCacheEnabled = 0x2D,
    WebAudioEnabled = 0x2E,
    Unk2F = 0x2F,
    YouTubeVideoWhitelist = 0x31,
    FooterFixedKind = 0x32,
    PageFadeEnabled = 0x33,
    MediaCreatorApplicationRatingAge = 0x34,
    BootLoadingIconEnabled = 0x35,
    PageScrollIndicationEnabled = 0x36,
    MediaPlayerSpeedControlEnabled = 0x37,
    AlbumEntry1 = 0x38,
    AlbumEntry2 = 0x39,
    AlbumEntry3 = 0x3A,
    AdditionalMediaData1 = 0x3B,
    AdditionalMediaData2 = 0x3C,
    AdditionalMediaData3 = 0x3D,
    BootFooterButton = 0x3E,
    OverrideWebAudioVolume = 0x3F,
    OverrideMediaAudioVolume = 0x40,
    BootMode = 0x41,
    WebSessionEnabled = 0x42,
};

enum class ShimKind : u32 {
    Shop = 1,
    Login = 2,
    Offline = 3,
    Share = 4,
    Web = 5,
    Wifi = 6,
    Lobby = 7,
};

constexpr std::size_t SHIM_KIND_COUNT = 0x8;

struct WebArgHeader {
    u16 count;
    INSERT_PADDING_BYTES(2);
    ShimKind kind;
};
static_assert(sizeof(WebArgHeader) == 0x8, "WebArgHeader has incorrect size.");

struct WebArgTLV {
    WebArgTLVType type;
    u16 size;
    u32 offset;
};
static_assert(sizeof(WebArgTLV) == 0x8, "WebArgTLV has incorrect size.");

struct WebCommonReturnValue {
    u32 result_code;
    INSERT_PADDING_BYTES(0x4);
    std::array<char, 0x1000> last_url;
    u64 last_url_size;
};
static_assert(sizeof(WebCommonReturnValue) == 0x1010, "WebCommonReturnValue has incorrect size.");

struct WebWifiPageArg {
    INSERT_PADDING_BYTES(4);
    std::array<char, 0x100> connection_test_url;
    std::array<char, 0x400> initial_url;
    std::array<u8, 0x10> nifm_network_uuid;
    u32 nifm_requirement;
};
static_assert(sizeof(WebWifiPageArg) == 0x518, "WebWifiPageArg has incorrect size.");

struct WebWifiReturnValue {
    INSERT_PADDING_BYTES(4);
    u32 result;
};
static_assert(sizeof(WebWifiReturnValue) == 0x8, "WebWifiReturnValue has incorrect size.");

enum class OfflineWebSource : u32 {
    OfflineHtmlPage = 0x1,
    ApplicationLegalInformation = 0x2,
    SystemDataPage = 0x3,
};

enum class ShopWebTarget {
    ApplicationInfo,
    AddOnContentList,
    SubscriptionList,
    ConsumableItemList,
    Home,
    Settings,
};

namespace {

std::map<WebArgTLVType, std::vector<u8>> GetWebArguments(const std::vector<u8>& arg) {
    WebArgHeader header{};
    if (arg.size() < sizeof(WebArgHeader))
        return {};

    std::memcpy(&header, arg.data(), sizeof(WebArgHeader));

    std::map<WebArgTLVType, std::vector<u8>> out;
    u64 offset = sizeof(WebArgHeader);
    for (std::size_t i = 0; i < header.count; ++i) {
        WebArgTLV tlv{};
        if (arg.size() < (offset + sizeof(WebArgTLV)))
            return out;

        std::memcpy(&tlv, arg.data() + offset, sizeof(WebArgTLV));
        offset += sizeof(WebArgTLV);

        offset += tlv.offset;
        if (arg.size() < (offset + tlv.size))
            return out;

        std::vector<u8> data(tlv.size);
        std::memcpy(data.data(), arg.data() + offset, tlv.size);
        offset += tlv.size;

        out.insert_or_assign(tlv.type, data);
    }

    return out;
}

FileSys::VirtualFile GetApplicationRomFS(u64 title_id, FileSys::ContentRecordType type) {
    const auto& installed{Core::System::GetInstance().GetContentProvider()};
    const auto res = installed.GetEntry(title_id, type);

    if (res != nullptr) {
        return res->GetRomFS();
    }

    if (type == FileSys::ContentRecordType::Data) {
        return FileSys::SystemArchive::SynthesizeSystemArchive(title_id);
    }

    return nullptr;
}

} // Anonymous namespace

WebBrowser::WebBrowser(Core::Frontend::WebBrowserApplet& frontend,
                       Core::Frontend::ECommerceApplet* frontend_e_commerce)
    : frontend(frontend), frontend_e_commerce(frontend_e_commerce) {}

WebBrowser::~WebBrowser() = default;

void WebBrowser::Initialize() {
    Applet::Initialize();

    complete = false;
    temporary_dir.clear();
    filename.clear();
    status = RESULT_SUCCESS;

    const auto web_arg_storage = broker.PopNormalDataToApplet();
    ASSERT(web_arg_storage != nullptr);
    const auto& web_arg = web_arg_storage->GetData();

    ASSERT(web_arg.size() >= 0x8);
    std::memcpy(&kind, web_arg.data() + 0x4, sizeof(ShimKind));

    args = GetWebArguments(web_arg);

    InitializeInternal();
}

bool WebBrowser::TransactionComplete() const {
    return complete;
}

ResultCode WebBrowser::GetStatus() const {
    return status;
}

void WebBrowser::ExecuteInteractive() {
    UNIMPLEMENTED_MSG("Unexpected interactive data recieved!");
}

void WebBrowser::Execute() {
    if (complete)
        return;

    if (status != RESULT_SUCCESS) {
        complete = true;
        return;
    }

    frontend.OpenPage(filename, [this] { UnpackRomFS(); }, [this] { Finalize(); });
}

void WebBrowser::UnpackRomFS() {
    if (unpacked)
        return;

    ASSERT(manual_romfs != nullptr);
    const auto dir =
        FileSys::ExtractRomFS(manual_romfs, FileSys::RomFSExtractionType::SingleDiscard);
    const auto& vfs{Core::System::GetInstance().GetFilesystem()};
    const auto temp_dir = vfs->CreateDirectory(temporary_dir, FileSys::Mode::ReadWrite);
    FileSys::VfsRawCopyD(dir, temp_dir);

    unpacked = true;
}

void WebBrowser::Finalize() {
    complete = true;

    WebArgumentResult out{};
    out.result_code = 0;
    out.last_url_size = 0;

    std::vector<u8> data(sizeof(WebArgumentResult));
    std::memcpy(data.data(), &out, sizeof(WebArgumentResult));

    broker.PushNormalDataFromApplet(IStorage{data});
    broker.SignalStateChanged();

    FileUtil::DeleteDirRecursively(temporary_dir);
}

} // namespace Service::AM::Applets
