
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Nintendo {

enum class AuthenticationState {
    NotAuthenticated,
    InProgress,
    Authenticated,
    Failed
};

enum class LibraryError {
    None,
    NetworkError,
    AuthenticationFailed,
    ParseError,
    InvalidCredentials,
    ServiceUnavailable
};

struct GameInfo {
    std::string title_id;
    std::string title_name;
    std::string platform;
    std::string purchase_date;
    std::string image_url;
    bool is_digital;
};

class Library {
public:
    Library();
    ~Library();

    // Core lifecycle
    bool Initialize();
    void Shutdown();

    // Authentication management
    bool StartAuthentication(const std::string& username, const std::string& password);
    bool IsAuthenticationInProgress() const;
    AuthenticationState GetAuthenticationState() const;
    LibraryError GetLastError() const;
    
    // Game library management
    std::vector<GameInfo> GetGameList();
    bool RefreshGameList();
    void ClearCache();
    
    // Configuration
    void SetCacheDirectory(const std::string& cache_dir);
    void SetUserAgent(const std::string& user_agent);
    
    // Status and diagnostics
    bool IsInitialized() const;
    std::string GetStatusMessage() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
    
    // Private helper methods
    void PerformAuthentication();
    std::string ExtractCSRFToken(const std::string& html);
    std::string UrlEncode(const std::string& value);
    std::vector<GameInfo> ParsePurchaseHistory(const std::string& html);
};

} // namespace Nintendo
