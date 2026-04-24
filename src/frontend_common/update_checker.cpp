// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <fmt/format.h>
#include "common/logging.h"
#include "common/scm_rev.h"
#include "update_checker.h"

#include "common/httplib.h"

#ifdef YUZU_BUNDLED_OPENSSL
#include <openssl/cert.h>
#endif

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

std::optional<std::string> UpdateChecker::GetResponse(std::string url, std::string path)
{
    try {
        constexpr std::size_t timeout_seconds = 15;

        std::unique_ptr<httplib::Client> client = std::make_unique<httplib::Client>(url);
        client->set_connection_timeout(timeout_seconds);
        client->set_read_timeout(timeout_seconds);
        client->set_write_timeout(timeout_seconds);

#ifdef YUZU_BUNDLED_OPENSSL
        client->load_ca_cert_store(kCert, sizeof(kCert));
#endif

        if (client == nullptr) {
            LOG_ERROR(Frontend, "Invalid URL {}{}", url, path);
            return {};
        }

        httplib::Request request{
            .method = "GET",
            .path = path,
        };

        client->set_follow_location(true);
        httplib::Result result = client->send(request);

        if (!result) {
            LOG_ERROR(Frontend, "GET to {}{} returned null", url, path);
            return {};
        }

        const auto &response = result.value();
        if (response.status >= 400) {
            LOG_ERROR(Frontend,
                      "GET to {}{} returned error status code: {}",
                      url,
                      path,
                      response.status);
            return {};
        }
        if (!response.headers.contains("content-type")) {
            LOG_ERROR(Frontend, "GET to {}{} returned no content", url, path);
            return {};
        }

        return response.body;
    } catch (std::exception &e) {
        LOG_ERROR(Frontend,
                  "GET to {}{} failed during update check: {}",
                  url,
                  path,
                  e.what());
        return std::nullopt;
    }
}

std::optional<UpdateChecker::Update> UpdateChecker::GetLatestRelease() {
#ifdef YUZU_BUNDLED_OPENSSL
    const auto update_check_url = fmt::format("https://{}", Common::g_build_auto_update_api);
#else
    const auto update_check_url = std::string{Common::g_build_auto_update_api};
#endif

    auto update_check_path = std::string{Common::g_build_auto_update_api_path};
    try {
        const auto response = UpdateChecker::GetResponse(update_check_url, update_check_path);

        if (!response)
            return {};

        const std::string latest_tag = nlohmann::json::parse(response.value()).at("tag_name");
        const std::string latest_name = nlohmann::json::parse(response.value()).at("name");

        return Update{latest_tag, latest_name};
    } catch (nlohmann::detail::out_of_range&) {
        LOG_ERROR(Frontend,
                  "Parsing JSON response from {}{} failed during update check: "
                  "nlohmann::detail::out_of_range",
                  update_check_url,
                  update_check_path);
        return {};
    } catch (nlohmann::detail::type_error&) {
        LOG_ERROR(Frontend,
                  "Parsing JSON response from {}{} failed during update check: "
                  "nlohmann::detail::type_error",
                  update_check_url,
                  update_check_path);
        return {};
    }
}

std::optional<UpdateChecker::Update> UpdateChecker::GetUpdate() {
    const std::optional<UpdateChecker::Update> latest_release_tag =
        UpdateChecker::GetLatestRelease();

    if (!latest_release_tag)
        goto empty;

    {
        std::string tag, build;
        if (Common::g_is_nightly_build) {
            std::vector<std::string> result;

            boost::split(result, latest_release_tag->tag, boost::is_any_of("."));
            if (result.size() != 2)
                goto empty;
            tag = result[1];

            boost::split(result, std::string{Common::g_build_version}, boost::is_any_of("-"));
            if (result.empty())
                goto empty;
            build = result[0];
        } else {
            tag = latest_release_tag->tag;
            build = Common::g_build_version;
        }

        if (tag != build)
            return latest_release_tag.value();
    }

empty:
    return std::nullopt;
}
