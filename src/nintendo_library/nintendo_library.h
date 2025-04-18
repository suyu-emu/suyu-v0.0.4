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

    // Add more methods as needed

private:
    // Add private members for internal state
    bool initialized;
    std::string current_rom;
    // Add more members as needed
};

} // namespace Nintendo