#pragma once

#include <string>
#include <memory>

// Forward declaration
namespace Nintendo {
class Library;
}

struct retro_game_info;

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

private:
    void* core_handle;
    retro_game_info game_info;
    std::unique_ptr<Nintendo::Library> nintendo_library;

    // Libretro function pointers
    void (*retro_init)();
    void (*retro_deinit)();
    unsigned (*retro_api_version)();
    void (*retro_get_system_info)(struct retro_system_info *info);
    void (*retro_get_system_av_info)(struct retro_system_av_info *info);
    void (*retro_set_environment)(void (*)(unsigned, const char*));
    void (*retro_set_video_refresh)(void (*)(const void*, unsigned, unsigned, size_t));
    void (*retro_set_audio_sample)(void (*)(int16_t, int16_t));
    void (*retro_set_audio_sample_batch)(size_t (*)(const int16_t*, size_t));
    void (*retro_set_input_poll)(void (*)());
    void (*retro_set_input_state)(int16_t (*)(unsigned, unsigned, unsigned, unsigned));
    void (*retro_set_controller_port_device)(unsigned, unsigned);
    void (*retro_reset)();
    void (*retro_run)();
    size_t (*retro_serialize_size)();
    bool (*retro_serialize)(void*, size_t);
    bool (*retro_unserialize)(const void*, size_t);
    bool (*retro_load_game)(const struct retro_game_info*);
    void (*retro_unload_game)();
};

} // namespace Core