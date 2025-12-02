#include "nintendo_library.h"

#ifdef USE_HTTPLIB
#include <httplib.h>
#elif defined(USE_CURL)
#include <curl/curl.h>
#endif

#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <iomanip>

#include "common/logging/log.h"

namespace Nintendo {

#ifdef USE_CURL
// Helper function for CURL write callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}
#endif

// Implementation class using PIMPL pattern
class Library::Impl {
public:
    Impl() : auth_state(AuthenticationState::NotAuthenticated), 
             last_error(LibraryError::None),
             initialized(false)
#ifdef USE_CURL
             , curl_handle(nullptr)
#endif
    {
        user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
    }

    ~Impl() {
        Cleanup();
    }

    bool Initialize() {
        if (initialized) {
            return true;
        }

#ifdef USE_HTTPLIB
        // httplib is header-only, no initialization needed
        initialized = true;
        last_error = LibraryError::None;
        status_message = "Nintendo Library initialized successfully (using cpp-httplib)";
        LOG_INFO(Service_Nintendo, "Nintendo Library initialized with cpp-httplib");
        return true;
#elif defined(USE_CURL)
        // Initialize CURL
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_handle = curl_easy_init();
        
        if (!curl_handle) {
            last_error = LibraryError::NetworkError;
            status_message = "Failed to initialize HTTP client";
            LOG_ERROR(Service_Nintendo, "Failed to initialize CURL");
            return false;
        }

        // Set up CURL options
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, user_agent.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, ""); // Enable cookie engine
        
        initialized = true;
        last_error = LibraryError::None;
        status_message = "Nintendo Library initialized successfully (using CURL)";
        
        LOG_INFO(Service_Nintendo, "Nintendo Library initialized with CURL");
        return true;
#else
        // No HTTP library available
        initialized = true;
        last_error = LibraryError::ServiceUnavailable;
        status_message = "Nintendo Library initialized (network features disabled - no HTTP library)";
        LOG_WARNING(Service_Nintendo, "Nintendo Library initialized without HTTP support");
        return true;
#endif
    }

    void Shutdown() {
        Cleanup();
        initialized = false;
        auth_state = AuthenticationState::NotAuthenticated;
        status_message = "Nintendo Library shut down";
        LOG_INFO(Service_Nintendo, "Nintendo Library shut down");
    }

private:
    void Cleanup() {
#ifdef USE_CURL
        if (curl_handle) {
            curl_easy_cleanup(curl_handle);
            curl_handle = nullptr;
        }
        curl_global_cleanup();
#endif
    }

public:
    // Member variables
    AuthenticationState auth_state;
    LibraryError last_error;
    bool initialized;
#ifdef USE_CURL
    CURL* curl_handle;
#endif
    std::string user_agent;
    std::string cache_directory;
    std::string status_message;
    std::string username;
    std::string password;
    std::vector<GameInfo> cached_games;
    std::string session_cookies;
};

// Library class implementation
Library::Library() : impl(std::make_unique<Impl>()) {}

Library::~Library() = default;

bool Library::Initialize() {
    return impl->Initialize();
}

void Library::Shutdown() {
    impl->Shutdown();
}

bool Library::StartAuthentication(const std::string& username, const std::string& password) {
    if (!impl->initialized) {
        impl->last_error = LibraryError::ServiceUnavailable;
        impl->status_message = "Library not initialized";
        return false;
    }

    if (username.empty() || password.empty()) {
        impl->last_error = LibraryError::InvalidCredentials;
        impl->status_message = "Username and password cannot be empty";
        return false;
    }

    impl->username = username;
    impl->password = password;
    impl->auth_state = AuthenticationState::InProgress;
    impl->last_error = LibraryError::None;
    impl->status_message = "Starting authentication...";

    LOG_INFO(Service_Nintendo, "Starting Nintendo account authentication for user: {}", username);

    // Perform authentication in a separate thread to avoid blocking
    std::thread auth_thread([this]() {
        PerformAuthentication();
    });
    auth_thread.detach();

    return true;
}

void Library::PerformAuthentication() {
    if (!impl->curl_handle) {
        impl->auth_state = AuthenticationState::Failed;
        impl->last_error = LibraryError::NetworkError;
        impl->status_message = "HTTP client not available";
        return;
    }

    try {
        // Step 1: Get Nintendo login page
        std::string login_url = "https://accounts.nintendo.com/login";
        std::string response;
        
        curl_easy_setopt(impl->curl_handle, CURLOPT_URL, login_url.c_str());
        curl_easy_setopt(impl->curl_handle, CURLOPT_WRITEDATA, &response);
        
        CURLcode res = curl_easy_perform(impl->curl_handle);
        if (res != CURLE_OK) {
            impl->auth_state = AuthenticationState::Failed;
            impl->last_error = LibraryError::NetworkError;
            impl->status_message = "Failed to connect to Nintendo login page";
            LOG_ERROR(Service_Nintendo, "CURL error: {}", curl_easy_strerror(res));
            return;
        }

        // Step 2: Extract CSRF token and other form data
        std::string csrf_token = ExtractCSRFToken(response);
        if (csrf_token.empty()) {
            impl->auth_state = AuthenticationState::Failed;
            impl->last_error = LibraryError::ParseError;
            impl->status_message = "Failed to extract authentication token";
            return;
        }

        // Step 3: Perform login
        std::string login_data = "authenticity_token=" + csrf_token + 
                                "&user%5Bemail%5D=" + UrlEncode(impl->username) +
                                "&user%5Bpassword%5D=" + UrlEncode(impl->password);

        response.clear();
        curl_easy_setopt(impl->curl_handle, CURLOPT_URL, "https://accounts.nintendo.com/login");
        curl_easy_setopt(impl->curl_handle, CURLOPT_POSTFIELDS, login_data.c_str());
        curl_easy_setopt(impl->curl_handle, CURLOPT_WRITEDATA, &response);

        res = curl_easy_perform(impl->curl_handle);
        if (res != CURLE_OK) {
            impl->auth_state = AuthenticationState::Failed;
            impl->last_error = LibraryError::NetworkError;
            impl->status_message = "Login request failed";
            return;
        }

        // Step 4: Check if login was successful
        long response_code;
        curl_easy_getinfo(impl->curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
        
        if (response_code == 200 && response.find("error") == std::string::npos) {
            impl->auth_state = AuthenticationState::Authenticated;
            impl->last_error = LibraryError::None;
            impl->status_message = "Authentication successful";
            LOG_INFO(Service_Nintendo, "Nintendo account authentication successful");
        } else {
            impl->auth_state = AuthenticationState::Failed;
            impl->last_error = LibraryError::AuthenticationFailed;
            impl->status_message = "Invalid username or password";
            LOG_WARNING(Service_Nintendo, "Nintendo account authentication failed");
        }

    } catch (const std::exception& e) {
        impl->auth_state = AuthenticationState::Failed;
        impl->last_error = LibraryError::NetworkError;
        impl->status_message = "Authentication error: " + std::string(e.what());
        LOG_ERROR(Service_Nintendo, "Authentication exception: {}", e.what());
    }
}

std::string Library::ExtractCSRFToken(const std::string& html) {
    // Look for authenticity_token in the HTML
    std::regex token_regex(R"(name="authenticity_token"[^>]*value="([^"]+)")");
    std::smatch match;
    
    if (std::regex_search(html, match, token_regex)) {
        return match[1].str();
    }
    
    return "";
}

std::string Library::UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}

bool Library::IsAuthenticationInProgress() const {
    return impl->auth_state == AuthenticationState::InProgress;
}

AuthenticationState Library::GetAuthenticationState() const {
    return impl->auth_state;
}

LibraryError Library::GetLastError() const {
    return impl->last_error;
}

std::vector<GameInfo> Library::GetGameList() {
    if (impl->auth_state != AuthenticationState::Authenticated) {
        impl->last_error = LibraryError::AuthenticationFailed;
        impl->status_message = "Not authenticated";
        return {};
    }

    if (!impl->cached_games.empty()) {
        return impl->cached_games;
    }

    return RefreshGameList() ? impl->cached_games : std::vector<GameInfo>{};
}

bool Library::RefreshGameList() {
    if (impl->auth_state != AuthenticationState::Authenticated) {
        impl->last_error = LibraryError::AuthenticationFailed;
        impl->status_message = "Not authenticated";
        return false;
    }

    if (!impl->curl_handle) {
        impl->last_error = LibraryError::NetworkError;
        impl->status_message = "HTTP client not available";
        return false;
    }

    try {
        // Use the correct Nintendo orders URL as specified in the issue
        std::string orders_url = "https://www.nintendo.com/us/orders/";
        std::string response;
        
        curl_easy_setopt(impl->curl_handle, CURLOPT_URL, orders_url.c_str());
        curl_easy_setopt(impl->curl_handle, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(impl->curl_handle, CURLOPT_HTTPGET, 1L); // Reset to GET

        CURLcode res = curl_easy_perform(impl->curl_handle);
        if (res != CURLE_OK) {
            impl->last_error = LibraryError::NetworkError;
            impl->status_message = "Failed to retrieve purchase history";
            LOG_ERROR(Service_Nintendo, "Failed to get orders page: {}", curl_easy_strerror(res));
            return false;
        }

        // Parse the purchase history HTML
        impl->cached_games = ParsePurchaseHistory(response);
        
        if (impl->cached_games.empty()) {
            impl->status_message = "No games found in purchase history";
            LOG_INFO(Service_Nintendo, "No games found in Nintendo purchase history");
        } else {
            impl->status_message = "Found " + std::to_string(impl->cached_games.size()) + " games";
            LOG_INFO(Service_Nintendo, "Found {} games in Nintendo purchase history", impl->cached_games.size());
        }

        impl->last_error = LibraryError::None;
        return true;

    } catch (const std::exception& e) {
        impl->last_error = LibraryError::ParseError;
        impl->status_message = "Error parsing purchase history: " + std::string(e.what());
        LOG_ERROR(Service_Nintendo, "Parse error: {}", e.what());
        return false;
    }
}

std::vector<GameInfo> Library::ParsePurchaseHistory(const std::string& html) {
    std::vector<GameInfo> games;
    
    try {
        // Look for game entries in the purchase history
        // Nintendo's purchase history typically contains game titles and purchase dates
        std::regex game_regex(R"(<div[^>]*class="[^"]*order-item[^"]*"[^>]*>.*?</div>)", std::regex_constants::icase);
        std::regex title_regex(R"(<h[0-9][^>]*>([^<]+)</h[0-9]>|<span[^>]*class="[^"]*title[^"]*"[^>]*>([^<]+)</span>)", std::regex_constants::icase);
        std::regex date_regex(R"((\d{1,2}/\d{1,2}/\d{4}|\d{4}-\d{2}-\d{2}))", std::regex_constants::icase);
        std::regex platform_regex(R"(Nintendo Switch|3DS|Wii U|Wii)", std::regex_constants::icase);
        
        std::sregex_iterator games_begin(html.begin(), html.end(), game_regex);
        std::sregex_iterator games_end;
        
        for (std::sregex_iterator i = games_begin; i != games_end; ++i) {
            std::string game_html = i->str();
            GameInfo game_info;
            
            // Extract title
            std::smatch title_match;
            if (std::regex_search(game_html, title_match, title_regex)) {
                game_info.title_name = title_match[1].str().empty() ? title_match[2].str() : title_match[1].str();
                // Clean up title
                game_info.title_name = std::regex_replace(game_info.title_name, std::regex(R"(\s+)"), " ");
                game_info.title_name = std::regex_replace(game_info.title_name, std::regex(R"(^\s+|\s+$)"), "");
            }
            
            // Extract purchase date
            std::smatch date_match;
            if (std::regex_search(game_html, date_match, date_regex)) {
                game_info.purchase_date = date_match[1].str();
            }
            
            // Extract platform
            std::smatch platform_match;
            if (std::regex_search(game_html, platform_match, platform_regex)) {
                game_info.platform = platform_match[0].str();
            } else {
                game_info.platform = "Nintendo Switch"; // Default assumption
            }
            
            // Generate a simple title ID (this would need to be more sophisticated in a real implementation)
            if (!game_info.title_name.empty()) {
                std::hash<std::string> hasher;
                size_t hash = hasher(game_info.title_name);
                std::stringstream ss;
                ss << std::hex << hash;
                game_info.title_id = ss.str().substr(0, 16);
                
                game_info.is_digital = true; // Assume digital purchases from web store
                games.push_back(game_info);
            }
        }
        
        // If no games found with the primary regex, try a simpler approach
        if (games.empty()) {
            // Look for any text that might be game titles
            std::regex simple_title_regex(R"(<[^>]*>([^<]*(?:Mario|Zelda|Pokemon|Metroid|Kirby|Splatoon|Animal Crossing|Fire Emblem|Xenoblade)[^<]*)</[^>]*>)", std::regex_constants::icase);
            std::sregex_iterator titles_begin(html.begin(), html.end(), simple_title_regex);
            std::sregex_iterator titles_end;
            
            for (std::sregex_iterator i = titles_begin; i != titles_end; ++i) {
                GameInfo game_info;
                game_info.title_name = i->str(1);
                game_info.platform = "Nintendo Switch";
                game_info.is_digital = true;
                
                std::hash<std::string> hasher;
                size_t hash = hasher(game_info.title_name);
                std::stringstream ss;
                ss << std::hex << hash;
                game_info.title_id = ss.str().substr(0, 16);
                
                games.push_back(game_info);
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR(Service_Nintendo, "Error parsing purchase history: {}", e.what());
    }
    
    return games;
}

void Library::ClearCache() {
    impl->cached_games.clear();
    impl->status_message = "Cache cleared";
}

void Library::SetCacheDirectory(const std::string& cache_dir) {
    impl->cache_directory = cache_dir;
    
    // Create cache directory if it doesn't exist
    try {
        if (!cache_dir.empty()) {
            std::filesystem::create_directories(cache_dir);
        }
    } catch (const std::exception& e) {
        LOG_WARNING(Service_Nintendo, "Failed to create cache directory: {}", e.what());
    }
}

void Library::SetUserAgent(const std::string& user_agent) {
    impl->user_agent = user_agent;
    if (impl->curl_handle) {
        curl_easy_setopt(impl->curl_handle, CURLOPT_USERAGENT, user_agent.c_str());
    }
}

bool Library::IsInitialized() const {
    return impl->initialized;
}

std::string Library::GetStatusMessage() const {
    return impl->status_message;
}

} // namespace Nintendo