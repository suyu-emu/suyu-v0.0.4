#include "picosha2.h"
#include "cppcodec/base64_rfc4648.hpp"
#include "uuid.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <map>

namespace Nintendo {

class Library {
public:
    Library() : initialized(false), accessToken("") {}
    ~Library() {
        if (initialized) {
            Shutdown();
        }
    }
    bool Initialize() {
        if (initialized) {
            return true;
        }
        std::cout << "Nintendo Library initialized" << std::endl;
        initialized = true;
        return true;
    }
    void Shutdown() {
        if (!initialized) {
            return;
        }
        std::cout << "Nintendo Library shut down" << std::endl;
        initialized = false;
    }
    bool LoadROM(const std::string& rom_path) {
        if (!initialized) {
            std::cerr << "Nintendo Library not initialized" << std::endl;
            return false;
        }
        current_rom = rom_path;
        std::cout << "ROM loaded: " << rom_path << std::endl;
        return true;
    }
    bool RunFrame() {
        if (!initialized || current_rom.empty()) {
            std::cerr << "Cannot run frame: Library not initialized or no ROM loaded" << std::endl;
            return false;
        }
        return true;
    }
    void SetVideoBuffer(void* buffer, int width, int height) {
        std::cout << "Video buffer set: " << width << "x" << height << std::endl;
    }
    void SetAudioBuffer(void* buffer, int size) {
        std::cout << "Audio buffer set: " << size << " bytes" << std::endl;
    }
    std::string StartAuthentication() {
        code_verifier = generate_code_verifier();
        std::vector<unsigned char> hash = picosha2::hash256(code_verifier.begin(), code_verifier.end());
        std::string code_challenge = cppcodec::base64_url_unpadded::encode(hash.begin(), hash.end());
        state = uuid::generate_uuid_v4();
        // Replace with actual values
        std::string redirect_uri = "npf71b963c1b7b6d119://auth";
        std::string client_id = "your_client_id";
        std::string scope = "user";
        std::string authorization_url = "https://accounts.nintendo.com/connect/1.0.0/authorize?state=" + state + "&redirect_uri=" + redirect_uri + "&client_id=" + client_id + "&scope=" + scope + "&response_type=code&code_challenge=" + code_challenge + "&code_challenge_method=S256";
        return authorization_url;
    }
    bool CompleteAuthentication(const std::string& redirect_url, const std::string& naBirthday, const std::string& f) {
        size_t hash_pos = redirect_url.find('#');
        if (hash_pos != std::string::npos) {
            std::string fragment = redirect_url.substr(hash_pos + 1);
            std::map<std::string, std::string> params;
            size_t pos = 0;
            while ((pos = fragment.find('&', pos)) != std::string::npos) {
                std::string pair = fragment.substr(0, pos);
                size_t eq_pos = pair.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = pair.substr(0, eq_pos);
                    std::string value = pair.substr(eq_pos + 1);
                    params[key] = value;
                }
                fragment = fragment.substr(pos + 1);
                pos = 0;
            }
            size_t eq_pos = fragment.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = fragment.substr(0, eq_pos);
                std::string value = fragment.substr(eq_pos + 1);
                params[key] = value;
            }
            auto it_state = params.find("state");
            if (it_state == params.end() || it_state->second != state) {
                std::cerr << "State mismatch" << std::endl;
                return false;
            }
            auto it_session_state = params.find("session_state");
            if (it_session_state == params.end()) {
                std::cerr << "Session state not found" << std::endl;
                return false;
            }
            std::string session_state = it_session_state->second;
            nlohmann::json session_token_data;
            session_token_data["session_state"] = session_state;
            session_token_data["state"] = state;
            std::string session_token_url = "https://accounts.nintendo.com/connect/1.0.0/api/session_token";
            std::string session_token_response = http_post(session_token_url, session_token_data.dump());
            nlohmann::json session_token_json = nlohmann::json::parse(session_token_response);
            std::string session_token = session_token_json["session_token"];
            nlohmann::json token_data;
            token_data["session_token"] = session_token;
            std::string token_url = "https://accounts.nintendo.com/connect/1.0.0/api/token";
            std::string token_response = http_post(token_url, token_data.dump());
            nlohmann::json token_json = nlohmann::json::parse(token_response);
            std::string id_token = token_json["id_token"];
            auto now = std::chrono::system_clock::now();
            auto now_c = std::chrono::system_clock::to_time_t(now);
            std::string timestamp = std::to_string(now_c);
            std::string requestId = uuid::generate_uuid_v4();
            nlohmann::json login_data;
            login_data["naIdToken"] = id_token;
            login_data["naBirthday"] = naBirthday;
            login_data["timestamp"] = timestamp;
            login_data["requestId"] = requestId;
            login_data["f"] = f;
            std::string login_url = "https://api-lp1.znc.srv.nintendo.net/v1/Account/Login";
            std::string login_response = http_post(login_url, login_data.dump());
            nlohmann::json login_json = nlohmann::json::parse(login_response);
            accessToken = login_json["webApiServerCredential"]["accessToken"];
            return true;
        } else {
            std::cerr << "No fragment in redirect URL" << std::endl;
            return false;
        }
    }
    std::vector<std::string> GetGameList() {
        if (accessToken.empty()) {
            std::cerr << "Not authenticated" << std::endl;
            return {};
        }
        CURL* curl = curl_easy_init();
        if (!curl) {
            return {};
        }
        std::string url = "https://api-lp1.znc.srv.nintendo.net/v1/Game/ListWebServices";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        std::string response_string;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
            return {};
        }
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        nlohmann::json response_json = nlohmann::json::parse(response_string);
        std::vector<std::string> game_list;
        for (const auto& game : response_json["data"]["gameList"]) {
            game_list.push_back(game["gameId"].get<std::string>());
        }
        return game_list;
    }
private:
    bool initialized;
    std::string current_rom;
    std::string code_verifier;
    std::string state;
    std::string accessToken;
};

std::string generate_code_verifier() {
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.size() - 1);
    std::string code_verifier;
    for (int i = 0; i < 43; ++i) {
        code_verifier += chars[dis(gen)];
    }
    return code_verifier;
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string http_post(const std::string& url, const std::string& data) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return "";
    }
    std::string response_string;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
    }
    curl_easy_cleanup(curl);
    return response_string;
}

} // namespace Nintendo
