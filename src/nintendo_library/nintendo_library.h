
#pragma once

#include <string>
#include <vector>

namespace Nintendo {

class Library {
public:
    Library();
    ~Library();

    bool Initialize();
    void Shutdown();

    // Add methods for Nintendo-specific functionality
    bool LoadROM(const std::string& rom_path);
    bool RunFrame();
    void SetVideoBuffer(void* buffer, int width, int height);
    void SetAudioBuffer(void* buffer, int size);

    struct GameInfo
    {
        std::string titleId;
        std::string titleName;
    };

    // Authentication and purchase-history APIs
    bool StartAuthentication(const std::string& username, const std::string& password);
    bool CompleteAuthentication(const std::string& twoFactorToken);
    std::vector<GameInfo> GetGameList();

    // Add more methods as needed

private:
    // Authentication state
    std::string authToken;
    std::string cookieJarPath;

    // Add private members for internal state
    bool initialized;
    std::string current_rom;
    // Add more members as needed
};

} // namespace Nintendo
