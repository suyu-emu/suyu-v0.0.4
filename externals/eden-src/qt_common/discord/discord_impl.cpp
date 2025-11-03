// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2018 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>
#include <string>

#include <QEventLoop>
#include <boost/algorithm/string/replace.hpp>
#include <httplib.h>

#include <discord_rpc.h>
#include <fmt/format.h>

#include "common/common_types.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/loader/loader.h"

#include "discord_impl.h"

#ifdef YUZU_BUNDLED_OPENSSL
#include <openssl/cert.h>
#endif

namespace DiscordRPC {

DiscordImpl::DiscordImpl(Core::System& system_) : system{system_} {
    DiscordEventHandlers handlers{};
    // The number is the client ID for Eden, it's used for images and the
    // application name
    Discord_Initialize("1397286652128264252", &handlers, 1, nullptr);
}

DiscordImpl::~DiscordImpl() {
    Discord_ClearPresence();
    Discord_Shutdown();
}

void DiscordImpl::Pause() {
    Discord_ClearPresence();
}

std::string DiscordImpl::GetGameString(const std::string& title) {
    // Convert to lowercase
    std::string icon_name = Common::ToLower(title);

    // Replace spaces with dashes
    std::replace(icon_name.begin(), icon_name.end(), ' ', '-');
    boost::replace_all(icon_name, "é", "e");

    // Remove non-alphanumeric characters but keep dashes
    std::erase_if(icon_name, [](char c) { return !std::isalnum(c) && c != '-'; });

    // Remove dashes from the start and end of the string
    icon_name.erase(icon_name.begin(), std::find_if(icon_name.begin(), icon_name.end(),
                                                    [](int ch) { return ch != '-'; }));
    icon_name.erase(
        std::find_if(icon_name.rbegin(), icon_name.rend(), [](int ch) { return ch != '-'; }).base(),
        icon_name.end());

    // Remove double dashes
    icon_name.erase(std::unique(icon_name.begin(), icon_name.end(),
                                [](char a, char b) { return a == '-' && b == '-'; }),
                    icon_name.end());

    return icon_name;
}

void DiscordImpl::UpdateGameStatus(bool use_default) {
    const std::string default_text = "Eden is an emulator for the Nintendo Switch";
    const std::string default_image = "https://git.eden-emu.dev/eden-emu/eden/raw/branch/master/"
                                      "dist/qt_themes/default/icons/256x256/eden.png";
    const std::string url = use_default ? default_image : game_url;
    s64 start_time = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    DiscordRichPresence presence{};

    presence.largeImageKey = url.c_str();
    presence.largeImageText = game_title.c_str();
    presence.smallImageKey = default_image.c_str();
    presence.smallImageText = default_text.c_str();
    presence.state = game_title.c_str();
    presence.details = "Currently in game";
    presence.startTimestamp = start_time;
    Discord_UpdatePresence(&presence);
}

void DiscordImpl::Update() {
    const std::string default_text = "Eden is an emulator for the Nintendo Switch";
    const std::string default_image = "https://git.eden-emu.dev/eden-emu/eden/raw/branch/master/"
                                      "dist/qt_themes/default/icons/256x256/eden.png";

    if (system.IsPoweredOn()) {
        system.GetAppLoader().ReadTitle(game_title);

        // Used to format Icon URL for yuzu website game compatibility page
        std::string icon_name = GetGameString(game_title);
        game_url = fmt::format(
            "https://raw.githubusercontent.com/eden-emulator/boxart/refs/heads/master/img/{}.png",
            icon_name);

        httplib::SSLClient client(game_url);
        client.set_connection_timeout(3);
        client.set_read_timeout(3);
        client.set_follow_location(true);

#ifdef YUZU_BUNDLED_OPENSSL
        client.load_ca_cert_store(kCert, sizeof(kCert));
#endif

        httplib::Request request{
            .method = "HEAD",
            .path = game_url,
        };

        auto res = client.send(request);
        UpdateGameStatus(res && res->status == 200);

        return;
    }

    s64 start_time = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

    DiscordRichPresence presence{};
    presence.largeImageKey = default_image.c_str();
    presence.largeImageText = default_text.c_str();
    presence.details = "Currently not in game";
    presence.startTimestamp = start_time;
    Discord_UpdatePresence(&presence);
}
} // namespace DiscordRPC
