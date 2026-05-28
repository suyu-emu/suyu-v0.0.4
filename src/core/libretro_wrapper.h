#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "common/dynamic_library.h"

// Forward declaration
namespace Nintendo {
class Library;
}

#ifndef SUYU_RETRO_GAME_INFO_DEFINED
#define SUYU_RETRO_GAME_INFO_DEFINED
struct retro_game_info {
    const char* path{};
    const void* data{};
    std::size_t size{};
    const char* meta{};
};
#endif

struct retro_system_info;
struct retro_system_av_info;

namespace Core {

class LibretroWrapper {
public:
    LibretroWrapper();
    ~LibretroWrapper();

    bool LoadCore(const std::string& core_path);
    bool LoadGame(const std::string& game_path);
    void Run();
    void Reset();
    void Unload();

    // Nintendo Library integration
    bool InitializeNintendoLibrary();
    bool AuthenticateNintendoAccount(const std::string& username, const std::string& password);
    std::vector<std::string> GetNintendoGameTitles();

private:
    Common::DynamicLibrary core_library;
    bool game_loaded = false;
    std::string loaded_game_path;
    retro_game_info game_info;
    std::unique_ptr<Nintendo::Library> nintendo_library;

    // Libretro function pointers
    void (*retro_init)() = nullptr;
    void (*retro_deinit)() = nullptr;
    unsigned (*retro_api_version)() = nullptr;
    void (*retro_get_system_info)(struct retro_system_info* info) = nullptr;
    void (*retro_get_system_av_info)(struct retro_system_av_info* info) = nullptr;
    void (*retro_set_environment)(void (*)(unsigned, const char*)) = nullptr;
    void (*retro_set_video_refresh)(void (*)(const void*, unsigned, unsigned, size_t)) = nullptr;
    void (*retro_set_audio_sample)(void (*)(int16_t, int16_t)) = nullptr;
    void (*retro_set_audio_sample_batch)(size_t (*)(const int16_t*, size_t)) = nullptr;
    void (*retro_set_input_poll)(void (*)()) = nullptr;
    void (*retro_set_input_state)(int16_t (*)(unsigned, unsigned, unsigned, unsigned)) = nullptr;
    void (*retro_set_controller_port_device)(unsigned, unsigned) = nullptr;
    void (*retro_reset)() = nullptr;
    void (*retro_run)() = nullptr;
    size_t (*retro_serialize_size)() = nullptr;
    bool (*retro_serialize)(void*, size_t) = nullptr;
    bool (*retro_unserialize)(const void*, size_t) = nullptr;
    bool (*retro_load_game)(const struct retro_game_info*) = nullptr;
    void (*retro_unload_game)() = nullptr;
};

} // namespace Core
