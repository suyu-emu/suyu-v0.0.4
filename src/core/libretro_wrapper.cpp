#include "core/libretro_wrapper.h"
#include "nintendo_library/nintendo_library.h"
#include <dlfcn.h>
#include <stdexcept>
#include <cstring>
#include <iostream>

namespace Core {

LibretroWrapper::LibretroWrapper() : core_handle(nullptr), nintendo_library(std::make_unique<Nintendo::Library>()) {}

LibretroWrapper::~LibretroWrapper() {
    Unload();
}

bool LibretroWrapper::LoadCore(const std::string& core_path) {
    core_handle = dlopen(core_path.c_str(), RTLD_LAZY);
    if (!core_handle) {
        std::cerr << "Failed to load libretro core: " << dlerror() << std::endl;
        return false;
    }

    // Load libretro core functions
    #define LOAD_SYMBOL(S) S = reinterpret_cast<decltype(S)>(dlsym(core_handle, #S)); \
    if (!S) { \
        std::cerr << "Failed to load symbol " #S ": " << dlerror() << std::endl; \
        Unload(); \
        return false; \
    }

    LOAD_SYMBOL(retro_init)
    LOAD_SYMBOL(retro_deinit)
    LOAD_SYMBOL(retro_api_version)
    LOAD_SYMBOL(retro_get_system_info)
    LOAD_SYMBOL(retro_get_system_av_info)
    LOAD_SYMBOL(retro_set_environment)
    LOAD_SYMBOL(retro_set_video_refresh)
    LOAD_SYMBOL(retro_set_audio_sample)
    LOAD_SYMBOL(retro_set_audio_sample_batch)
    LOAD_SYMBOL(retro_set_input_poll)
    LOAD_SYMBOL(retro_set_input_state)
    LOAD_SYMBOL(retro_set_controller_port_device)
    LOAD_SYMBOL(retro_reset)
    LOAD_SYMBOL(retro_run)
    LOAD_SYMBOL(retro_serialize_size)
    LOAD_SYMBOL(retro_serialize)
    LOAD_SYMBOL(retro_unserialize)
    LOAD_SYMBOL(retro_load_game)
    LOAD_SYMBOL(retro_unload_game)

    #undef LOAD_SYMBOL

    if (!nintendo_library->Initialize()) {
        std::cerr << "Failed to initialize Nintendo Library" << std::endl;
        Unload();
        return false;
    }

    retro_init();
    return true;
}

bool LibretroWrapper::LoadGame(const std::string& game_path) {
    if (!core_handle) {
        std::cerr << "Libretro core not loaded" << std::endl;
        return false;
    }

    game_info.path = game_path.c_str();
    game_info.data = nullptr;
    game_info.size = 0;
    game_info.meta = nullptr;

    if (!retro_load_game(&game_info)) {
        std::cerr << "Failed to load game through libretro" << std::endl;
        return false;
    }

    if (!nintendo_library->LoadROM(game_path)) {
        std::cerr << "Failed to load ROM through Nintendo Library" << std::endl;
        return false;
    }

    return true;
}

void LibretroWrapper::Run() {
    if (core_handle) {
        retro_run();
        nintendo_library->RunFrame();
    } else {
        std::cerr << "Cannot run: Libretro core not loaded" << std::endl;
    }
}

void LibretroWrapper::Reset() {
    if (core_handle) {
        retro_reset();
        // Add any necessary reset logic for Nintendo Library
    } else {
        std::cerr << "Cannot reset: Libretro core not loaded" << std::endl;
    }
}

void LibretroWrapper::Unload() {
    if (core_handle) {
        retro_unload_game();
        retro_deinit();
        dlclose(core_handle);
        core_handle = nullptr;
    }
    nintendo_library->Shutdown();
}

// Add implementations for other libretro functions as needed

} // namespace Core