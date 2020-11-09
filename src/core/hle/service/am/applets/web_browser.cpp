// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <optional>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend/applets/web_browser.h"
#include "core/hle/result.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/applets/web_browser.h"

namespace Service::AM::Applets {

namespace {

template <typename T>
void ParseRawValue(T& value, const std::vector<u8>& data) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "It's undefined behavior to use memcpy with non-trivially copyable objects");
    std::memcpy(&value, data.data(), data.size());
}

template <typename T>
T ParseRawValue(const std::vector<u8>& data) {
    T value;
    ParseRawValue(value, data);
    return value;
}

std::string ParseStringValue(const std::vector<u8>& data) {
    return Common::StringFromFixedZeroTerminatedBuffer(reinterpret_cast<const char*>(data.data()),
                                                       data.size());
}

WebArgInputTLVMap ReadWebArgs(const std::vector<u8>& web_arg, WebArgHeader& web_arg_header) {
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

std::optional<std::vector<u8>> GetInputTLVData(const WebArgInputTLVMap& input_tlv_map,
                                               WebArgInputTLVType input_tlv_type) {
    const auto map_it = input_tlv_map.find(input_tlv_type);

    if (map_it == input_tlv_map.end()) {
        return std::nullopt;
    }

    return map_it->second;
}

} // namespace

WebBrowser::WebBrowser(Core::System& system_, const Core::Frontend::WebBrowserApplet& frontend_)
    : Applet{system_.Kernel()}, frontend(frontend_), system{system_} {}

WebBrowser::~WebBrowser() = default;

void WebBrowser::Initialize() {
    Applet::Initialize();

    LOG_INFO(Service_AM, "Initializing Web Browser Applet.");

    LOG_DEBUG(Service_AM,
              "Initializing Applet with common_args: arg_version={}, lib_version={}, "
              "play_startup_sound={}, size={}, system_tick={}, theme_color={}",
              common_args.arguments_version, common_args.library_version,
              common_args.play_startup_sound, common_args.size, common_args.system_tick,
              common_args.theme_color);

    web_applet_version = WebAppletVersion{common_args.library_version};

    const auto web_arg_storage = broker.PopNormalDataToApplet();
    ASSERT(web_arg_storage != nullptr);

    const auto& web_arg = web_arg_storage->GetData();
    ASSERT_OR_EXECUTE(web_arg.size() >= sizeof(WebArgHeader), { return; });

    web_arg_input_tlv_map = ReadWebArgs(web_arg, web_arg_header);

    LOG_DEBUG(Service_AM, "WebArgHeader: total_tlv_entries={}, shim_kind={}",
              web_arg_header.total_tlv_entries, web_arg_header.shim_kind);

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
    default:
        UNREACHABLE_MSG("Invalid ShimKind={}", web_arg_header.shim_kind);
        break;
    }
}

bool WebBrowser::TransactionComplete() const {
    return complete;
}

ResultCode WebBrowser::GetStatus() const {
    return status;
}

void WebBrowser::ExecuteInteractive() {
    UNIMPLEMENTED_MSG("WebSession is not implemented");
}

void WebBrowser::Execute() {
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
    default:
        UNREACHABLE_MSG("Invalid ShimKind={}", web_arg_header.shim_kind);
        WebBrowserExit(WebExitReason::EndButtonPressed);
        break;
    }
}

void WebBrowser::WebBrowserExit(WebExitReason exit_reason, std::string last_url) {
    if ((web_arg_header.shim_kind == ShimKind::Share &&
         web_applet_version >= WebAppletVersion::Version196608) ||
        (web_arg_header.shim_kind == ShimKind::Web &&
         web_applet_version >= WebAppletVersion::Version524288)) {
        // TODO: Push Output TLVs instead of a WebCommonReturnValue
    }

    WebCommonReturnValue web_common_return_value;

    web_common_return_value.exit_reason = exit_reason;
    std::memcpy(&web_common_return_value.last_url, last_url.data(), last_url.size());
    web_common_return_value.last_url_size = last_url.size();

    LOG_DEBUG(Service_AM, "WebCommonReturnValue: exit_reason={}, last_url={}, last_url_size={}",
              exit_reason, last_url, last_url.size());

    complete = true;
    std::vector<u8> out_data(sizeof(WebCommonReturnValue));
    std::memcpy(out_data.data(), &web_common_return_value, out_data.size());
    broker.PushNormalDataFromApplet(std::make_shared<IStorage>(system, std::move(out_data)));
    broker.SignalStateChanged();
}

void WebBrowser::InitializeShop() {}

void WebBrowser::InitializeLogin() {}

void WebBrowser::InitializeOffline() {}

void WebBrowser::InitializeShare() {}

void WebBrowser::InitializeWeb() {}

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
    LOG_WARNING(Service_AM, "(STUBBED) called, Offline Applet is not implemented");
    WebBrowserExit(WebExitReason::EndButtonPressed);
}

void WebBrowser::ExecuteShare() {
    LOG_WARNING(Service_AM, "(STUBBED) called, Share Applet is not implemented");
    WebBrowserExit(WebExitReason::EndButtonPressed);
}

void WebBrowser::ExecuteWeb() {
    LOG_WARNING(Service_AM, "(STUBBED) called, Web Applet is not implemented");
    WebBrowserExit(WebExitReason::EndButtonPressed);
}

void WebBrowser::ExecuteWifi() {
    LOG_WARNING(Service_AM, "(STUBBED) called, Wifi Applet is not implemented");
    WebBrowserExit(WebExitReason::EndButtonPressed);
}

void WebBrowser::ExecuteLobby() {
    LOG_WARNING(Service_AM, "(STUBBED) called, Lobby Applet is not implemented");
    WebBrowserExit(WebExitReason::EndButtonPressed);
}

} // namespace Service::AM::Applets
