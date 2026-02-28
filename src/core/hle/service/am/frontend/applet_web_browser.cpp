// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/fs_filesystem.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/system_archive/system_archive.h"
#include "core/file_sys/vfs/vfs_vector.h"
#include "core/frontend/applets/web_browser.h"
#include "core/hle/result.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/frontend/applet_web_browser.h"
#include "core/hle/service/am/service/storage.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/ns/platform_service_manager.h"
#include "core/loader/loader.h"

namespace Service::AM::Frontend {

namespace {

template <typename T>
void ParseRawValue(T& value, std::span<const u8> data) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "It's undefined behavior to use memcpy with non-trivially copyable objects");
    std::memcpy(&value, data.data(), data.size());
}

template <typename T>
T ParseRawValue(std::span<const u8> data) {
    T value;
    ParseRawValue(value, data);
    return value;
}

std::string ParseStringValue(std::span<const u8> data) {
    return Common::StringFromFixedZeroTerminatedBuffer(reinterpret_cast<const char*>(data.data()), data.size());
}

std::string GetMainURL(const std::string& url) {
    const auto index = url.find('?');

    if (index == std::string::npos) {
        return url;
    }

    return url.substr(0, index);
}

std::string ResolveURL(const std::string& url) {
    const auto index = url.find_first_of('%');

    if (index == std::string::npos) {
        return url;
    }

    return url.substr(0, index) + "lp1" + url.substr(index + 1);
}

WebArgInputTLVMap ReadWebArgs(std::span<const u8> web_arg, WebArgHeader& web_arg_header) {
    std::memcpy(&web_arg_header, web_arg.data(), sizeof(WebArgHeader));

    if (web_arg.size() == sizeof(WebArgHeader)) {
        return {};
    }

    WebArgInputTLVMap input_tlv_map;

    u64 current_offset = sizeof(WebArgHeader);

    for (std::size_t i = 0; i < web_arg_header.total_tlv_entries; ++i) {
        if (web_arg.size() < current_offset + sizeof(WebArgInputTLV)) {
            return input_tlv_map;
        }

        WebArgInputTLV input_tlv;
        std::memcpy(&input_tlv, web_arg.data() + current_offset, sizeof(WebArgInputTLV));

        current_offset += sizeof(WebArgInputTLV);

        if (web_arg.size() < current_offset + input_tlv.arg_data_size) {
            return input_tlv_map;
        }

        std::vector<u8> data(input_tlv.arg_data_size);
        std::memcpy(data.data(), web_arg.data() + current_offset, input_tlv.arg_data_size);

        current_offset += input_tlv.arg_data_size;

        input_tlv_map.insert_or_assign(input_tlv.input_tlv_type, std::move(data));
    }

    return input_tlv_map;
}

FileSys::VirtualFile GetOfflineRomFS(Core::System& system, u64 title_id,
                                     FileSys::ContentRecordType nca_type) {
    if (nca_type == FileSys::ContentRecordType::Data) {
        const auto nca =
            system.GetFileSystemController().GetSystemNANDContents()->GetEntry(title_id, nca_type);

        if (nca == nullptr) {
            LOG_ERROR(Service_AM,
                      "NCA of type={} with title_id={:016X} is not found in the System NAND!",
                      nca_type, title_id);
            return FileSys::SystemArchive::SynthesizeSystemArchive(title_id);
        }

        return nca->GetRomFS();
    } else {
        const auto nca = system.GetContentProvider().GetEntry(title_id, nca_type);

        if (nca == nullptr) {
            if (nca_type == FileSys::ContentRecordType::HtmlDocument) {
                LOG_WARNING(Service_AM, "Falling back to AppLoader to get the RomFS.");
                FileSys::VirtualFile romfs;
                system.GetAppLoader().ReadManualRomFS(romfs);
                if (romfs != nullptr) {
                    return romfs;
                }
            }

            LOG_ERROR(Service_AM,
                      "NCA of type={} with title_id={:016X} is not found in the ContentProvider!",
                      nca_type, title_id);
            return nullptr;
        }

        const FileSys::PatchManager pm{title_id, system.GetFileSystemController(),
                                       system.GetContentProvider()};

        return pm.PatchRomFS(nca.get(), nca->GetRomFS(), nca_type);
    }
}

void ExtractSharedFonts(Core::System& system) {
    static constexpr std::array<const char*, 7> DECRYPTED_SHARED_FONTS{
        "FontStandard.ttf",
        "FontChineseSimplified.ttf",
        "FontExtendedChineseSimplified.ttf",
        "FontChineseTraditional.ttf",
        "FontKorean.ttf",
        "FontNintendoExtended.ttf",
        "FontNintendoExtended2.ttf",
    };

    const auto fonts_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "fonts";

    for (std::size_t i = 0; i < NS::SHARED_FONTS.size(); ++i) {
        const auto font_file_path = fonts_dir / DECRYPTED_SHARED_FONTS[i];

        if (Common::FS::Exists(font_file_path)) {
            continue;
        }

        const auto font = NS::SHARED_FONTS[i];
        const auto font_title_id = static_cast<u64>(font.first);

        const auto nca = system.GetFileSystemController().GetSystemNANDContents()->GetEntry(
            font_title_id, FileSys::ContentRecordType::Data);

        FileSys::VirtualFile romfs;

        if (!nca) {
            romfs = FileSys::SystemArchive::SynthesizeSystemArchive(font_title_id);
        } else {
            romfs = nca->GetRomFS();
        }

        if (!romfs) {
            LOG_ERROR(Service_AM, "SharedFont RomFS with title_id={:016X} cannot be extracted!",
                      font_title_id);
            continue;
        }

        const auto extracted_romfs = FileSys::ExtractRomFS(romfs);

        if (!extracted_romfs) {
            LOG_ERROR(Service_AM, "SharedFont RomFS with title_id={:016X} failed to extract!",
                      font_title_id);
            continue;
        }

        const auto font_file = extracted_romfs->GetFile(font.second);

        if (!font_file) {
            LOG_ERROR(Service_AM, "SharedFont RomFS with title_id={:016X} has no font file \"{}\"!",
                      font_title_id, font.second);
            continue;
        }

        std::vector<u32> font_data_u32(font_file->GetSize() / sizeof(u32));
        font_file->ReadBytes<u32>(font_data_u32.data(), font_file->GetSize());

        std::transform(font_data_u32.begin(), font_data_u32.end(), font_data_u32.begin(),
                       Common::swap32);

        std::vector<u8> decrypted_data(font_file->GetSize() - 8);

        NS::DecryptSharedFontToTTF(font_data_u32, decrypted_data);

        FileSys::VirtualFile decrypted_font = std::make_shared<FileSys::VectorVfsFile>(
            std::move(decrypted_data), DECRYPTED_SHARED_FONTS[i]);

        const auto temp_dir = system.GetFilesystem()->CreateDirectory(
            Common::FS::PathToUTF8String(fonts_dir), FileSys::OpenMode::ReadWrite);

        const auto out_file = temp_dir->CreateFile(DECRYPTED_SHARED_FONTS[i]);

        FileSys::VfsRawCopy(decrypted_font, out_file);
    }
}

} // namespace

WebBrowser::WebBrowser(Core::System& system_, std::shared_ptr<Applet> applet_,
                       LibraryAppletMode applet_mode_,
                       const Core::Frontend::WebBrowserApplet& frontend_)
    : FrontendApplet{system_, applet_, applet_mode_}, frontend(frontend_) {}

WebBrowser::~WebBrowser() = default;

void WebBrowser::Initialize() {
    FrontendApplet::Initialize();

    LOG_INFO(Service_AM, "Initializing Web Browser Applet.");

    LOG_DEBUG(Service_AM,
              "Initializing Applet with common_args: arg_version={}, lib_version={}, "
              "play_startup_sound={}, size={}, system_tick={}, theme_color={}",
              common_args.arguments_version, common_args.library_version,
              common_args.play_startup_sound, common_args.size, common_args.system_tick,
              common_args.theme_color);

    web_applet_version = WebAppletVersion{common_args.library_version};

    const auto web_arg_storage = PopInData();
    ASSERT(web_arg_storage != nullptr);

    const auto& web_arg = web_arg_storage->GetData();
    ASSERT_OR_EXECUTE(web_arg.size() >= sizeof(WebArgHeader), { return; });

    web_arg_input_tlv_map = ReadWebArgs(web_arg, web_arg_header);

    LOG_DEBUG(Service_AM, "WebArgHeader: total_tlv_entries={}, shim_kind={}",
              web_arg_header.total_tlv_entries, web_arg_header.shim_kind);

    if (Settings::values.disable_web_applet &&
        web_arg_header.shim_kind != ShimKind::Web &&
        web_arg_header.shim_kind != ShimKind::Lhub) {
        return;
    }

    ExtractSharedFonts(system);

    switch (web_arg_header.shim_kind) {
    case ShimKind::Shop:
        InitializeShop();
        break;
    case ShimKind::Login:
        InitializeLogin();
        break;
    case ShimKind::Offline:
        InitializeOffline();
        break;
    case ShimKind::Share:
        InitializeShare();
        break;
    case ShimKind::Web:
        InitializeWeb();
        break;
    case ShimKind::Wifi:
        InitializeWifi();
        break;
    case ShimKind::Lobby:
        InitializeLobby();
        break;
    case ShimKind::Lhub:
        InitializeLhub();
        break;
    default:
        ASSERT_MSG(false, "Invalid ShimKind={}", web_arg_header.shim_kind);
        break;
    }
}

Result WebBrowser::GetStatus() const {
    return status;
}

void WebBrowser::ExecuteInteractive() {
    UNIMPLEMENTED_MSG("WebSession is not implemented");
}

void WebBrowser::Execute() {
    if (web_arg_header.shim_kind == ShimKind::Web) {
        ExecuteWeb();
        return;
    }

    if (web_arg_header.shim_kind == ShimKind::Lhub) {
        ExecuteLhub();
        return;
    }

    if (Settings::values.disable_web_applet) {
        LOG_WARNING(Service_AM, "(STUBBED) called, Web Browser Applet is disabled. shim_kind={}",
                    web_arg_header.shim_kind);
        WebBrowserExit(WebExitReason::EndButtonPressed);
        return;
    }

    switch (web_arg_header.shim_kind) {
    case ShimKind::Shop:
        ExecuteShop();
        break;
    case ShimKind::Login:
        ExecuteLogin();
        break;
    case ShimKind::Offline:
        ExecuteOffline();
        break;
    case ShimKind::Share:
        ExecuteShare();
        break;
    case ShimKind::Web:
        ExecuteWeb();
        break;
    case ShimKind::Wifi:
        ExecuteWifi();
        break;
    case ShimKind::Lobby:
        ExecuteLobby();
        break;
    case ShimKind::Lhub:
        ExecuteLhub();
        break;
    default:
        ASSERT_MSG(false, "Invalid ShimKind={}", web_arg_header.shim_kind);
        WebBrowserExit(WebExitReason::EndButtonPressed);
        break;
    }
}

void WebBrowser::ExtractOfflineRomFS() {
    LOG_DEBUG(Service_AM, "Extracting RomFS to {}",
              Common::FS::PathToUTF8String(offline_cache_dir));

    const auto extracted_romfs_dir = FileSys::ExtractRomFS(offline_romfs);

    const auto temp_dir = system.GetFilesystem()->CreateDirectory(
        Common::FS::PathToUTF8String(offline_cache_dir), FileSys::OpenMode::ReadWrite);

    FileSys::VfsRawCopyD(extracted_romfs_dir, temp_dir);
}

void WebBrowser::WebBrowserExit(WebExitReason exit_reason, std::string last_url) {
    const bool use_tlv_output =
        (web_arg_header.shim_kind == ShimKind::Share &&
         web_applet_version >= WebAppletVersion::Version196608) ||
        (web_arg_header.shim_kind == ShimKind::Web &&
         web_applet_version >= WebAppletVersion::Version524288) ||
        (web_arg_header.shim_kind == ShimKind::Lhub);

    // https://switchbrew.org/wiki/Internet_Browser#TLVs
    if (use_tlv_output) {
        LOG_DEBUG(Service_AM, "Using TLV output: exit_reason={}, last_url={}, last_url_size={}",
                  exit_reason, last_url, last_url.size());

        // storage size for TLVs is 0x2000 bytes (as per switchbrew documentation)
        constexpr size_t TLV_STORAGE_SIZE = 0x2000;
        std::vector<u8> out_data(TLV_STORAGE_SIZE, 0);

        size_t current_offset = sizeof(WebArgHeader);
        u16 tlv_count = 0;

        // align and matchng TLV struct alignment
        auto align_offset = [](size_t offset) -> size_t {
            return (offset + 7) & ~static_cast<size_t>(7);
        };

        // 0x1 ShareExitReason
        {
            WebArgOutputTLV tlv{};
            tlv.output_tlv_type = WebArgOutputTLVType::ShareExitReason;
            tlv.arg_data_size = sizeof(u32);

            std::memcpy(out_data.data() + current_offset, &tlv, sizeof(WebArgOutputTLV));
            current_offset += sizeof(WebArgOutputTLV);

            const u32 exit_reason_value = static_cast<u32>(exit_reason);
            std::memcpy(out_data.data() + current_offset, &exit_reason_value, sizeof(u32));
            current_offset += sizeof(u32);

            current_offset = align_offset(current_offset);
            tlv_count++;
        }

        // 0x2 LastUrl
        {
            WebArgOutputTLV tlv{};
            tlv.output_tlv_type = WebArgOutputTLVType::LastURL;
            const u16 url_data_size = static_cast<u16>(last_url.size() + 1);
            tlv.arg_data_size = url_data_size;

            std::memcpy(out_data.data() + current_offset, &tlv, sizeof(WebArgOutputTLV));
            current_offset += sizeof(WebArgOutputTLV);

            // null terminator
            std::memcpy(out_data.data() + current_offset, last_url.c_str(), last_url.size() + 1);
            current_offset += url_data_size;
            current_offset = align_offset(current_offset);
            tlv_count++;
        }

        // 0x3 LastUrlSize
        {
            WebArgOutputTLV tlv{};
            tlv.output_tlv_type = WebArgOutputTLVType::LastURLSize;
            tlv.arg_data_size = sizeof(u64);

            std::memcpy(out_data.data() + current_offset, &tlv, sizeof(WebArgOutputTLV));
            current_offset += sizeof(WebArgOutputTLV);

            const u64 url_size = last_url.size();
            std::memcpy(out_data.data() + current_offset, &url_size, sizeof(u64));
            current_offset += sizeof(u64);
            tlv_count++;
        }

        WebArgHeader out_header{};
        out_header.total_tlv_entries = tlv_count;
        out_header.shim_kind = web_arg_header.shim_kind;
        std::memcpy(out_data.data(), &out_header, sizeof(WebArgHeader));

        LOG_DEBUG(Service_AM, "TLV output: total_size={}, tlv_count={}, used_offset={}",
                  out_data.size(), tlv_count, current_offset);

        complete = true;
        PushOutData(std::make_shared<IStorage>(system, std::move(out_data)));
        Exit();
        return;
    }

    // for old browser, keep old return, use WebCommonReturnValue
    WebCommonReturnValue web_common_return_value{};

    web_common_return_value.exit_reason = exit_reason;
    std::memcpy(&web_common_return_value.last_url, last_url.data(),
                (std::min)(last_url.size(), web_common_return_value.last_url.size()));
    web_common_return_value.last_url_size = last_url.size();

    LOG_DEBUG(Service_AM, "WebCommonReturnValue: exit_reason={}, last_url={}, last_url_size={}",
              exit_reason, last_url, last_url.size());

    complete = true;
    std::vector<u8> out_data(sizeof(WebCommonReturnValue));
    std::memcpy(out_data.data(), &web_common_return_value, out_data.size());
    PushOutData(std::make_shared<IStorage>(system, std::move(out_data)));
    Exit();
}

Result WebBrowser::RequestExit() {
    frontend.Close();
    R_SUCCEED();
}

bool WebBrowser::InputTLVExistsInMap(WebArgInputTLVType input_tlv_type) const {
    return web_arg_input_tlv_map.find(input_tlv_type) != web_arg_input_tlv_map.end();
}

std::optional<std::vector<u8>> WebBrowser::GetInputTLVData(WebArgInputTLVType input_tlv_type) {
    const auto map_it = web_arg_input_tlv_map.find(input_tlv_type);

    if (map_it == web_arg_input_tlv_map.end()) {
        return std::nullopt;
    }

    return map_it->second;
}

void WebBrowser::InitializeShop() {}

void WebBrowser::InitializeLogin() {}

void WebBrowser::InitializeOffline() {
    const auto document_path =
        ParseStringValue(GetInputTLVData(WebArgInputTLVType::DocumentPath).value());

    const auto document_kind =
        ParseRawValue<DocumentKind>(GetInputTLVData(WebArgInputTLVType::DocumentKind).value());

    std::string additional_paths;

    switch (document_kind) {
    case DocumentKind::OfflineHtmlPage:
    default:
        title_id = system.GetApplicationProcessProgramID();
        nca_type = FileSys::ContentRecordType::HtmlDocument;
        additional_paths = "html-document";
        break;
    case DocumentKind::ApplicationLegalInformation:
        title_id = ParseRawValue<u64>(GetInputTLVData(WebArgInputTLVType::ApplicationID).value());
        nca_type = FileSys::ContentRecordType::LegalInformation;
        break;
    case DocumentKind::SystemDataPage:
        title_id = ParseRawValue<u64>(GetInputTLVData(WebArgInputTLVType::SystemDataID).value());
        nca_type = FileSys::ContentRecordType::Data;
        break;
    }

    static constexpr std::array<const char*, 3> RESOURCE_TYPES{
        "manual",
        "legal_information",
        "system_data",
    };

    offline_cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) /
                        fmt::format("offline_web_applet_{}/{:016X}",
                                    RESOURCE_TYPES[static_cast<u32>(document_kind) - 1], title_id);

    offline_document = Common::FS::ConcatPathSafe(
        offline_cache_dir, fmt::format("{}/{}", additional_paths, document_path));
}

void WebBrowser::InitializeShare() {}

void WebBrowser::InitializeWeb() {
    external_url = ParseStringValue(GetInputTLVData(WebArgInputTLVType::InitialURL).value());

    // Resolve Nintendo CDN URLs.
    external_url = ResolveURL(external_url);
}

void WebBrowser::InitializeWifi() {}

void WebBrowser::InitializeLobby() {}

void WebBrowser::ExecuteShop() {
    LOG_WARNING(Service_AM, "(STUBBED) called, Shop Applet is not implemented");
    WebBrowserExit(WebExitReason::EndButtonPressed);
}

void WebBrowser::ExecuteLogin() {
    LOG_WARNING(Service_AM, "(STUBBED) called, Login Applet is not implemented");
    WebBrowserExit(WebExitReason::EndButtonPressed);
}

void WebBrowser::ExecuteOffline() {
    // TODO (Morph): This is a hack for WebSession foreground web applets such as those used by
    //               Super Mario 3D All-Stars.
    // TODO (Morph): Implement WebSession.
    if (applet_mode == LibraryAppletMode::AllForegroundInitiallyHidden) {
        LOG_WARNING(Service_AM, "WebSession is not implemented");
        return;
    }

    const auto main_url = GetMainURL(Common::FS::PathToUTF8String(offline_document));

    if (!Common::FS::Exists(main_url)) {
        offline_romfs = GetOfflineRomFS(system, title_id, nca_type);

        if (offline_romfs == nullptr) {
            LOG_ERROR(Service_AM,
                      "RomFS with title_id={:016X} and nca_type={} cannot be extracted!", title_id,
                      nca_type);
            WebBrowserExit(WebExitReason::WindowClosed);
            return;
        }
    }

    LOG_INFO(Service_AM, "Opening offline document at {}",
             Common::FS::PathToUTF8String(offline_document));

    frontend.OpenLocalWebPage(
        Common::FS::PathToUTF8String(offline_document), [this] { ExtractOfflineRomFS(); },
        [this](WebExitReason exit_reason, std::string last_url) {
            WebBrowserExit(exit_reason, last_url);
        });
}

void WebBrowser::ExecuteShare() {
    LOG_WARNING(Service_AM, "(STUBBED) called, Share Applet is not implemented");
    WebBrowserExit(WebExitReason::EndButtonPressed);
}

void WebBrowser::ExecuteWeb() {
    LOG_INFO(Service_AM, "Opening external URL at {}", external_url);

    frontend.OpenExternalWebPage(external_url,
                                 [this](WebExitReason exit_reason, std::string last_url) {
                                     WebBrowserExit(exit_reason, last_url);
                                 });
}

void WebBrowser::ExecuteWifi() {
    LOG_WARNING(Service_AM, "(STUBBED) called, Wifi Applet is not implemented");
    WebBrowserExit(WebExitReason::EndButtonPressed);
}

void WebBrowser::ExecuteLobby() {
    LOG_WARNING(Service_AM, "(STUBBED) called, Lobby Applet is not implemented");
    WebBrowserExit(WebExitReason::EndButtonPressed);
}

void WebBrowser::InitializeLhub() {}

void WebBrowser::ExecuteLhub() {
    LOG_INFO(Service_AM, "(STUBBED) called, Lhub Applet is not implemented");
    WebBrowserExit(WebExitReason::EndButtonPressed);
}


} // namespace Service::AM::Frontend
