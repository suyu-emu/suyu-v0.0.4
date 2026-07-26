#pragma once

#include <cstddef>
#include <cstdint>
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

    // Latest frame/audio pulled from the core's callbacks, for a frontend
    // renderer/audio-backend to consume after each Run().
    struct VideoFrame {
        const void* data{};
        unsigned width{};
        unsigned height{};
        size_t pitch{};
    };
    [[nodiscard]] const VideoFrame& LastVideoFrame() const {
        return last_frame;
    }
    [[nodiscard]] const std::vector<int16_t>& LastAudioSamples() const {
        return last_audio;
    }

    // Savestate support (retro_serialize/unserialize passthrough).
    bool SaveState(std::vector<uint8_t>& out_data);
    bool LoadState(const std::vector<uint8_t>& in_data);

    // Called from the free-function libretro callback trampolines; not part
    // of the public frontend API but must be callable from outside the class.
    void OnVideoFrame(const void* data, unsigned width, unsigned height, size_t pitch);

    /// Pixel format the loaded core asked for (libretro RETRO_PIXEL_FORMAT_*).
    /// Defaults to 0 (0RGB1555), which is what the ABI specifies when a core
    /// never sets one.
    void SetPixelFormat(int format) {
        pixel_format = format;
    }
    [[nodiscard]] int GetPixelFormat() const {
        return pixel_format;
    }

    /// Directory handed to cores for BIOS/system files and saves. Stable for
    /// the lifetime of the wrapper so the pointer returned to a core stays
    /// valid.
    [[nodiscard]] const char* GetSystemDirectory() const {
        return system_directory.c_str();
    }
    void SetSystemDirectory(std::string dir) {
        system_directory = std::move(dir);
    }
    void OnAudioSamples(const int16_t* data, size_t frames);

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
    VideoFrame last_frame;
    std::vector<int16_t> last_audio;
    /// Pixel format the core requested; 0 (0RGB1555) is the ABI default.
    int pixel_format = 0;
    /// Backing store for the pointer handed to cores via
    /// GET_SYSTEM_DIRECTORY / GET_SAVE_DIRECTORY.
    std::string system_directory;

    // Libretro function pointers
    void (*retro_init)() = nullptr;
    void (*retro_deinit)() = nullptr;
    unsigned (*retro_api_version)() = nullptr;
    void (*retro_get_system_info)(struct retro_system_info* info) = nullptr;
    void (*retro_get_system_av_info)(struct retro_system_av_info* info) = nullptr;
    // Must match libretro's retro_environment_t exactly: bool(unsigned, void*).
    // It was previously declared as void(unsigned, const char*), so the
    // callback was invoked through the wrong function-pointer type and its
    // "result" was whatever happened to be in the return register - meaning
    // every environment query a core made effectively failed.
    void (*retro_set_environment)(bool (*)(unsigned, void*)) = nullptr;
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
