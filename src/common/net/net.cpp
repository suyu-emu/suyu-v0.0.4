// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <optional>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>

#include <fmt/format.h>
#include "common/scm_rev.h"
#include "net.h"

#include "common/logging.h"

#include "common/httplib.h"

#ifdef YUZU_BUNDLED_OPENSSL
#include <openssl/cert.h>
#endif

#define QT_TR_NOOP(x) x

namespace Common::Net {

std::vector<Asset> Release::GetPlatformAssets() const {
    // TODO(crueter): Need better handling for this as a whole.
#ifdef NIGHTLY_BUILD
    std::vector<std::string> result;
    boost::algorithm::split(result, tag, boost::is_any_of("."));
    if (result.size() != 2)
        return {};
    const auto ref = result.at(1);
#else
    const auto ref = tag;
#endif

    std::vector<Asset> found_assets;

    // FIXME: This is mildly inefficient.
    // Finds assets based on a hierarchy of regex search strings.
    const auto find_asset = [&found_assets, ref, this](const std::string& name,
                                                       const std::vector<std::string>& suffixes) {
        for (const std::string& asset : assets) {
            for (const auto& suffix : suffixes) {
                if (asset.ends_with(suffix)) {
                    const std::string_view asset_sv = asset;
                    const size_t pos = asset_sv.find_last_of('/');
                    const std::string_view filename =
                        (pos != std::string_view::npos) ? asset_sv.substr(pos + 1) : asset_sv;

                    found_assets.emplace_back(Asset{
                        .name = name,
                        .url = host,
                        .path = asset,
                        .filename = std::string{filename},
                    });
                    return;
                }
            }
        }
    };

#ifdef _WIN32
#ifdef ARCHITECTURE_x86_64
    find_asset("Standard", {"amd64-msvc-standard.exe", "amd64-msvc-standard.zip", "mingw-amd64-gcc-standard.exe", "mingw-amd64-gcc-standard.zip"});
    find_asset("PGO", {"mingw-amd64-clang-pgo.exe", "mingw-amd64-clang-pgo.zip"});
#elif defined(ARCHITECTURE_arm64)
    find_asset("Standard", {"mingw-arm64-clang-standard.exe", "mingw-arm64-clang-standard.zip"});
    find_asset("PGO", {"mingw-arm64-clang-pgo.exe", "mingw-arm64-clang-pgo.zip"});
#endif
#elif defined(__APPLE__)
#ifdef ARCHITECTURE_arm64
    find_asset("Standard", {".dmg", ".tar.gz"});
#endif
#elif defined(__ANDROID__)
#ifdef ARCHITECTURE_x86_64
    find_asset("Standard", {"chromeos.apk"});
#elif defined(ARCHITECTURE_arm64)
#ifdef YUZU_LEGACY
    find_asset("Standard", {"legacy.apk"});
#elif defined(GENSHIN_SPOOF)
    find_asset("Standard", {"optimized.apk"});
#else
    find_asset("Standard", {"standard.apk"});
#endif
#endif
#endif
    return found_assets;
}

static inline u64 ParseIsoTimestamp(const std::string& iso) {
    if (iso.empty())
        return 0;

    std::string buf = iso;
    if (buf.back() == 'Z')
        buf.pop_back();

    std::tm tm{};
    std::istringstream ss(buf);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail())
        return 0;

#ifdef _WIN32
    return static_cast<u64>(_mkgmtime(&tm));
#else
    return static_cast<u64>(timegm(&tm));
#endif
}

std::optional<Release> Release::FromJson(const nlohmann::json& json, const std::string& host,
                                         const std::string& repo) {
    Release rel;
    if (!json.is_object())
        return std::nullopt;

    rel.tag = json.value("tag_name", std::string{});
    if (rel.tag.empty())
        return std::nullopt;

    rel.title = json.value("name", rel.tag);
    rel.id = json.value("id", std::hash<std::string>{}(rel.title));

    rel.published = ParseIsoTimestamp(json.value("published_at", std::string{}));
    rel.prerelease = json.value("prerelease", false);

    auto body = json.value("body", rel.title);
    boost::replace_all(body, "\\r", "");
    boost::replace_all(body, "\\n", "\n");
    rel.body = body;

    rel.host = host;

    const auto release_base =
        fmt::format("{}/{}/releases", Common::g_build_auto_update_website, repo);
    const auto fallback_html = fmt::format("{}/tag/{}", release_base, rel.tag);
    rel.html_url = json.value("html_url", fallback_html);

    // This is our own "fake" API.
    if (json.contains("base")) {
        const auto base = json.value("base", fmt::format("https://{}", Common::g_build_auto_update_api));
        rel.base_download_url = fmt::format("{}/{}", base, rel.tag);

        // Assets are easy :)
        rel.assets = json.value("assets", std::vector<std::string>{});
    } else {
        const auto base_download_url = fmt::format("/{}/releases/download/{}", repo, rel.tag);

        rel.base_download_url = base_download_url;

        // assets are a bit more complex here. :(
        std::vector<std::string> assets;
        const nlohmann::json& arr = json["assets"];
        for (const auto &obj : arr) {
            const auto url = obj.value("browser_download_url", std::string{});
            assets.emplace_back(url);
        }

        rel.assets = assets;
    }

    return rel;
}

std::optional<Release> Release::FromJson(const std::string_view& json, const std::string& host,
                                         const std::string& repo) {
    try {
        return FromJson(nlohmann::json::parse(json), host, repo);
    } catch (std::exception& e) {
        LOG_WARNING(Common, "Failed to parse JSON: {}", e.what());
    }

    return {};
}

std::vector<Release> Release::ListFromJson(const nlohmann::json& json, const std::string& host,
                                           const std::string& repo) {
    if (!json.is_array())
        return {};

    std::vector<Release> releases;
    for (const auto& obj : json) {
        auto rel = Release::FromJson(obj, host, repo);
        if (rel)
            releases.emplace_back(rel.value());
    }
    return releases;
}

std::vector<Release> Release::ListFromJson(const std::string_view& json, const std::string& host,
                                           const std::string& repo) {
    try {
        return ListFromJson(nlohmann::json::parse(json), host, repo);
    } catch (std::exception& e) {
        LOG_WARNING(Common, "Failed to parse JSON: {}", e.what());
    }

    return {};
}

std::optional<std::string> MakeRequest(const std::string& url, const std::string& path) {
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
            LOG_ERROR(Common, "Invalid URL {}{}", url, path);
            return {};
        }

        httplib::Request request{
            .method = "GET",
            .path = path,
        };

        client->set_follow_location(true);
        httplib::Result result = client->send(request);

        if (!result) {
            LOG_ERROR(Common, "GET to {}{} returned null", url, path);
            return {};
        }

        const auto& response = result.value();
        if (response.status >= 400) {
            LOG_ERROR(Common, "GET to {}{} returned error status code: {}", url, path,
                      response.status);
            return {};
        }
        if (!response.headers.contains("content-type")) {
            LOG_ERROR(Common, "GET to {}{} returned no content", url, path);
            return {};
        }

        return response.body;
    } catch (std::exception& e) {
        LOG_ERROR(Common, "GET to {}{} failed during update check: {}", url, path, e.what());
        return std::nullopt;
    }
}

std::vector<Release> GetReleases() {
    const auto body = GetReleasesBody();

    if (!body) {
        LOG_WARNING(Common, "Failed to get stable releases");
        return {};
    }

    const std::string_view body_str = body.value();
    const auto url = fmt::format("https://{}", Common::g_build_auto_update_stable_api);
    return Release::ListFromJson(body_str, url, Common::g_build_auto_update_stable_repo);
}

std::optional<Release> GetLatestRelease() {
    const auto releases_path =  Common::g_build_auto_update_api_path;
    const auto url = fmt::format("https://{}", Common::g_build_auto_update_api);

    const auto body = MakeRequest(url, releases_path);
    if (!body) {
        LOG_WARNING(Common, "Failed to get latest release");
        return std::nullopt;
    }

    const std::string_view body_str = body.value();
    return Release::FromJson(body_str, url, Common::g_build_auto_update_repo);
}

std::optional<std::string> GetReleasesBody() {
    const auto releases_path =
        fmt::format("/{}/{}/releases", Common::g_build_auto_update_stable_api_path,
                    Common::g_build_auto_update_stable_repo);
    const auto url = fmt::format("https://{}", Common::g_build_auto_update_stable_api);

    return MakeRequest(url, releases_path);
}

} // namespace Common::Net
