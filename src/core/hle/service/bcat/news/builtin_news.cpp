// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/net/net.h"
#include "common/scm_rev.h"
#include "core/hle/service/bcat/news/builtin_news.h"
#include "core/hle/service/bcat/news/msgpack.h"
#include "core/hle/service/bcat/news/news_storage.h"

#include "common/fs/file.h"
#include "common/fs/path_util.h"
#include "common/logging.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/regex.hpp>
#include <boost/regex/v5/regex_replace.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "common/httplib.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <optional>
#include <thread>

#ifdef YUZU_BUNDLED_OPENSSL
#include <openssl/cert.h>
#endif

namespace Service::News {
namespace {

// Cached logo data
std::vector<u8> default_logo_small;
std::vector<u8> default_logo_large;
bool default_logos_loaded = false;

ankerl::unordered_dense::map<std::string, std::vector<u8>> news_images_small;
ankerl::unordered_dense::map<std::string, std::vector<u8>> news_images_large;
std::mutex images_mutex;


std::filesystem::path GetCachePath() {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "news" / "github_releases.json";
}

std::filesystem::path GetDefaultLogoPath(bool large) {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "news" /
           (large ? "eden_logo_large.jpg" : "eden_logo_small.jpg");
}

std::filesystem::path GetNewsImagePath(std::string_view news_id, bool large) {
    const std::string filename = fmt::format("{}_{}.jpg", news_id, large ? "large" : "small");
    return Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "news" / "images" / filename;
}

u32 HashToNewsId(std::string_view key) {
    return static_cast<u32>(std::hash<std::string_view>{}(key) & 0x7FFFFFFF);
}

std::vector<u8> TryLoadFromDisk(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return {};

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};

    const auto file_size = static_cast<std::streamsize>(f.tellg());
    if (file_size <= 0 || file_size > 10 * 1024 * 1024) return {};

    f.seekg(0);
    std::vector<u8> data(static_cast<size_t>(file_size));
    if (!f.read(reinterpret_cast<char*>(data.data()), file_size)) return {};

    return data;
}

// TODO(crueter): Migrate to use Common::Net
std::vector<u8> DownloadImage(const std::string& url_path, const std::filesystem::path& cache_path) {
    LOG_DEBUG(Service_BCAT, "Downloading image: https://eden-emu.dev{}", url_path);
    try {
        httplib::Client cli("https://eden-emu.dev");
        cli.set_follow_location(true);
        cli.set_connection_timeout(std::chrono::seconds(2));
        cli.set_read_timeout(std::chrono::seconds(2));

#ifdef YUZU_BUNDLED_OPENSSL
        cli.load_ca_cert_store(kCert, sizeof(kCert));
#endif

        if (auto res = cli.Get(url_path); res && res->status == 200 && !res->body.empty()) {
            std::vector<u8> data(res->body.begin(), res->body.end());

            std::error_code ec;
            std::filesystem::create_directories(cache_path.parent_path(), ec);
            if (std::ofstream out(cache_path, std::ios::binary); out) {
                out.write(res->body.data(), static_cast<std::streamsize>(res->body.size()));
            }
            return data;
        }
    } catch (...) {
        LOG_WARNING(Service_BCAT, "Failed to download: {}", url_path);
    }
    return {};
}

std::vector<u8> LoadDefaultLogo(bool large) {
    const auto path = GetDefaultLogoPath(large);
    const std::string url = large ? "/news/eden_logo_large.jpg" : "/news/eden_logo_small.jpg";

    auto data = TryLoadFromDisk(path);
    if (!data.empty()) return data;

    return DownloadImage(url, path);
}

void LoadDefaultLogos() {
    if (default_logos_loaded) return;
    default_logos_loaded = true;

    default_logo_small = LoadDefaultLogo(false);
    default_logo_large = LoadDefaultLogo(true);
}

std::vector<u8> GetNewsImage(std::string_view news_id, bool large) {
    const std::string id_str{news_id};

    {
        std::lock_guard lock{images_mutex};
        auto& cache = large ? news_images_large : news_images_small;
        if (auto it = cache.find(id_str); it != cache.end()) {
            return it->second;
        }
    }

    const auto cache_path = GetNewsImagePath(news_id, large);
    auto data = TryLoadFromDisk(cache_path);

    if (data.empty()) {
        const std::string url = fmt::format("/news/{}_{}.jpg", id_str, large ? "large" : "small");
        data = DownloadImage(url, cache_path);
    }

    if (data.empty()) {
        data = large ? default_logo_large : default_logo_small;
    }

    {
        std::lock_guard lock{images_mutex};
        auto& cache = large ? news_images_large : news_images_small;
        cache[id_str] = data;
    }

    return data;
}

void PreloadNewsImages(const std::vector<u32>& news_ids) {
    std::vector<std::future<void>> futures;
    futures.reserve(news_ids.size() * 2);

    for (const u32 id : news_ids) {
        const std::string id_str = fmt::format("{}", id);

        {
            std::lock_guard lock{images_mutex};
            if (news_images_small.contains(id_str) && news_images_large.contains(id_str)) {
                continue;
            }
        }

        const auto path_small = GetNewsImagePath(id_str, false);
        const auto path_large = GetNewsImagePath(id_str, true);
        if (std::filesystem::exists(path_small) && std::filesystem::exists(path_large)) {
            continue;
        }

        futures.push_back(std::async(std::launch::async, [id_str]() {
            GetNewsImage(id_str, false);
        }));
        futures.push_back(std::async(std::launch::async, [id_str]() {
            GetNewsImage(id_str, true);
        }));
    }

    for (auto& f : futures) {
        f.wait();
    }
}

std::optional<std::string> ReadCachedJson() {
    const auto path = GetCachePath();
    if (!std::filesystem::exists(path)) return std::nullopt;

    auto content = Common::FS::ReadStringFromFile(path, Common::FS::FileType::TextFile);
    return content.empty() ? std::nullopt : std::optional{std::move(content)};
}

void WriteCachedJson(std::string_view json) {
    const auto path = GetCachePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    (void)Common::FS::WriteStringToFile(path, Common::FS::FileType::TextFile, json);
}

// idk but News App does not render Markdown or HTML, so remove some formatting.
std::string SanitizeMarkdown(std::string_view markdown) {
    std::string result;
    result.reserve(markdown.size());

    // our current structure for markdown is after "# Packages" remove everything.
    std::string text{markdown};
    if (auto pos = text.find("# Packages"); pos != std::string::npos) {
        text = text.substr(0, pos);
    }

    // Fix line endings
    boost::replace_all(text, "\r", "");

    // Remove backticks
    boost::replace_all(text, "`", "");

    // Remove excessive newlines
    static const boost::regex newlines(R"(\n\n\n+)");
    text = boost::regex_replace(text, newlines, "\n\n");

    // Remove markdown headers
    static const boost::regex headers(R"(^#+ )");
    text = boost::regex_replace(text, headers, "");

    // Convert bullet points to something nicer
    static const boost::regex list1(R"(^- )");
    text = boost::regex_replace(text, list1, "• ");

    static const boost::regex list2(R"(^  \* )");
    text = boost::regex_replace(text, list2, "  • ");

    // what
    static const boost::regex list2_dash(R"(^ - )");
    text = boost::regex_replace(text, list2_dash, "  • ");

    // Convert bold/italic text into normal text
    static const boost::regex bold(R"(\*\*(.*?)\*\*)");
    text = boost::regex_replace(text, bold, "$1");

    static const boost::regex italic(R"(\*(.*?)\*)");
    text = boost::regex_replace(text, italic, "$1");

    // Remove links and convert to normal text
    static const boost::regex link(R"(\[([^\]]+)\]\([^)]*\))");
    text = boost::regex_replace(text, link, "$1");

    // Trim trailing whitespace/newlines
    while (!text.empty() && (text.back() == '\n' || text.back() == ' ')) {
        text.pop_back();
    }

    return text;
}

std::string FormatBody(std::string body, const std::string_view &title) {
    if (body.empty()) {
        return std::string(title);
    }

    // Sanitize markdown
    body = SanitizeMarkdown(body);

    // Limit body length - News app has character limits
    size_t max_body_length = 4000;
    if (body.size() > max_body_length) {
        size_t cut_pos = body.rfind('\n', max_body_length);
        if (cut_pos == std::string::npos || cut_pos < max_body_length / 2) {
            cut_pos = body.rfind(". ", max_body_length);
        }
        if (cut_pos == std::string::npos || cut_pos < max_body_length / 2) {
            cut_pos = max_body_length;
        }
        body = body.substr(0, cut_pos);

        // Trim trailing whitespace
        while (!body.empty() && (body.back() == '\n' || body.back() == ' ')) {
            body.pop_back();
        }

        body += "\n\n... View more on Forgejo";
    }

    return body;
}

void ImportReleases(const std::vector<Common::Net::Release> &releases) {
    std::vector<u32> news_ids;
    for (const auto& rel : releases) {
        const u32 news_id = u32(rel.id & 0x7FFFFFFF);
        news_ids.push_back(news_id);
    }

    PreloadNewsImages(news_ids);

    for (const auto& rel : releases) {
        const std::string title = rel.title;
        const std::string body = rel.body;
        const std::string html_url = rel.html_url;

        const u32 news_id = u32(rel.id & 0x7FFFFFFF);
        const u64 published = rel.published;
        const u64 pickup_limit = published + 600000000;
        const u32 priority = rel.prerelease ? 1500 : 2500;

        std::string author = "Eden";

        auto payload = BuildMsgpack(title, FormatBody(body, title), title, published,
                                    pickup_limit, priority, {"en"}, author, {},
                                    html_url, news_id);

        const std::string news_id_str = fmt::format("LA{:020}", rel.id);

        GithubNewsMeta meta{
            .news_id = news_id_str,
            .topic_id = "1",
            .published_at = published,
            .pickup_limit = pickup_limit,
            .essential_pickup_limit = pickup_limit,
            .expire_at = 0,
            .priority = priority,
            .deletion_priority = 100,
            .decoration_type = 1,
            .opted_in = 1,
            .essential_pickup_limit_flag = 1,
            .category = 0,
            .language_mask = 1,
        };

        NewsStorage::Instance().UpsertRaw(meta, std::move(payload));
    }
}

} // anonymous namespace

std::vector<u8> BuildMsgpack(std::string_view title, std::string_view body,
                             std::string_view topic_name, u64 published_at,
                             u64 pickup_limit, u32 priority,
                             const std::vector<std::string>& languages,
                             const std::string& author,
                             const std::vector<std::pair<std::string, std::string>>& /*assets*/,
                             const std::string& html_url,
                             std::optional<u32> override_id) {
    MsgPack::Writer w;

    const u32 news_id = override_id.value_or(HashToNewsId(title.empty() ? "eden" : title));
    const std::string news_id_str = fmt::format("{}", news_id);

    const auto img_small = GetNewsImage(news_id_str, false);
    const auto img_large = GetNewsImage(news_id_str, true);

    w.WriteFixMap(23);

    // Version infos, could exist a 2?
    w.WriteKey("version");
    w.WriteFixMap(2);
    w.WriteKey("format");
    w.WriteUInt(1);
    w.WriteKey("semantics");
    w.WriteUInt(1);

    // Metadata
    w.WriteKey("news_id");
    w.WriteUInt(news_id);
    w.WriteKey("published_at");
    w.WriteUInt(published_at);
    w.WriteKey("pickup_limit");
    w.WriteUInt(pickup_limit);
    w.WriteKey("priority");
    w.WriteUInt(priority);
    w.WriteKey("deletion_priority");
    w.WriteUInt(100);

    // Language
    w.WriteKey("language");
    w.WriteString(languages.empty() ? "en" : languages.front());
    w.WriteKey("supported_languages");
    w.WriteFixArray(languages.size());
    for (const auto& lang : languages) w.WriteString(lang);

    // Display settings
    w.WriteKey("display_type");
    w.WriteString("NORMAL");
    w.WriteKey("topic_id");
    w.WriteString("eden");

    w.WriteKey("no_photography"); // still show image
    w.WriteUInt(0);
    w.WriteKey("surprise"); // no idea
    w.WriteUInt(0);
    w.WriteKey("bashotorya"); // no idea
    w.WriteUInt(0);
    w.WriteKey("movie");
    w.WriteUInt(0); // 1 = has video, movie_url must be set but we don't support it yet

    // News Subject (Title)
    w.WriteKey("subject");
    w.WriteFixMap(2);
    w.WriteKey("caption");
    w.WriteUInt(1);
    w.WriteKey("text");
    w.WriteString(title.empty() ? "No title" : title);

    // Topic name = who wrote it
    w.WriteKey("topic_name");
    w.WriteString("Eden");

    w.WriteKey("list_image");
    w.WriteBinary(img_small);

    // Footer
    w.WriteKey("footer");
    w.WriteFixMap(1);
    w.WriteKey("text");
    w.WriteString("");

    w.WriteKey("allow_domains");
    w.WriteString("^https?://git.eden-emu.dev(/|$)");

    // More link
    w.WriteKey("more");
    w.WriteFixMap(1);
    w.WriteKey("browser");
    w.WriteFixMap(2);
    w.WriteKey("url");
    w.WriteString(html_url);
    w.WriteKey("text");
    w.WriteString("Open Forgejo");

    // Body
    w.WriteKey("body");
    w.WriteFixMap(4);
    w.WriteKey("text");
    w.WriteString(body);
    w.WriteKey("main_image_height");
    w.WriteUInt(450);
    w.WriteKey("movie_url");
    w.WriteString("");
    w.WriteKey("main_image");
    w.WriteBinary(img_large);

    // no clue
    w.WriteKey("contents_descriptors");
    w.WriteString("");
    w.WriteKey("interactive_elements");
    w.WriteString("");

    return w.Take();
}

void EnsureBuiltinNewsLoaded() {
    static std::once_flag once;
    std::call_once(once, [] {
        LoadDefaultLogos();

        if (const auto cached = ReadCachedJson()) {
            const std::string_view body = cached.value();
            const auto releases = Common::Net::Release::ListFromJson(body, Common::g_build_auto_update_stable_api, Common::g_build_auto_update_stable_repo);
            ImportReleases(releases);

            LOG_INFO(Service_BCAT, "news: {} entries loaded from cache", NewsStorage::Instance().ListAll().size());
        }

        std::thread([] {
            if (const auto fresh = Common::Net::GetReleasesBody()) {
                const std::string_view body = fresh.value();
                WriteCachedJson(body);
                const auto releases = Common::Net::Release::ListFromJson(body, Common::g_build_auto_update_stable_api, Common::g_build_auto_update_stable_repo);
                ImportReleases(releases);

                LOG_INFO(Service_BCAT, "news: {} entries updated from Forgejo", NewsStorage::Instance().ListAll().size());
            }
        }).detach();
    });
}

} // namespace Service::News
