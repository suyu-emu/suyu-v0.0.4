// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <fmt/format.h>
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "update_checker.h"

#include <httplib.h>

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

std::optional<UpdateChecker::Update> UpdateChecker::GetLatestRelease(bool include_prereleases) {
    const auto update_check_url = std::string{Common::g_build_auto_update_api};
    std::string update_check_path = fmt::format("/repos/{}",
                                                std::string{Common::g_build_auto_update_repo});
    try {
        if (include_prereleases) { // This can return either a prerelease or a stable release,
            // whichever is more recent.
            const auto update_check_tags_path = update_check_path + "/tags";
            const auto update_check_releases_path = update_check_path + "/releases";

            const auto tags_response = UpdateChecker::GetResponse(update_check_url, update_check_tags_path);
            const auto releases_response = UpdateChecker::GetResponse(update_check_url, update_check_releases_path);

            if (!tags_response || !releases_response)
                return {};

            const std::string latest_tag
                = nlohmann::json::parse(tags_response.value()).at(0).at("name");
            const std::string latest_name =
                nlohmann::json::parse(releases_response.value()).at(0).at("name");

            const bool latest_tag_has_release = releases_response.value().find(
                                                    fmt::format("\"{}\"", latest_tag))
                                                != std::string::npos;

            // If there is a newer tag, but that tag has no associated release, don't prompt the
            // user to update.
            if (!latest_tag_has_release)
                return {};

            return Update{latest_tag, latest_name};
        } else { // This is a stable release, only check for other stable releases.
            update_check_path += "/releases/latest";
            const auto response = UpdateChecker::GetResponse(update_check_url, update_check_path);

            if (!response)
                return {};

            const std::string latest_tag = nlohmann::json::parse(response.value()).at("tag_name");
            const std::string latest_name = nlohmann::json::parse(response.value()).at("name");

            return Update{latest_tag, latest_name};
        }

    } catch (nlohmann::detail::out_of_range &) {
        LOG_ERROR(Frontend,
                  "Parsing JSON response from {}{} failed during update check: "
                  "nlohmann::detail::out_of_range",
                  update_check_url,
                  update_check_path);
        return {};
    } catch (nlohmann::detail::type_error &) {
        LOG_ERROR(Frontend,
                  "Parsing JSON response from {}{} failed during update check: "
                  "nlohmann::detail::type_error",
                  update_check_url,
                  update_check_path);
        return {};
    }
}

std::optional<UpdateChecker::Update> UpdateChecker::GetUpdate() {
    const bool is_prerelease = ((strstr(Common::g_build_version, "pre-alpha") != NULL) ||
                                (strstr(Common::g_build_version, "alpha") != NULL) ||
                                (strstr(Common::g_build_version, "beta") != NULL) ||
                                (strstr(Common::g_build_version, "rc") != NULL));
    const std::optional<UpdateChecker::Update> latest_release_tag =
        UpdateChecker::GetLatestRelease(is_prerelease);

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
