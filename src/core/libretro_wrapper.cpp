#include "core/libretro_wrapper.h"
#include <dlfcn.h>
#include <stdexcept>
#include <cstring>

namespace Core {

LibretroWrapper::LibretroWrapper() : core_handle(nullptr) {}

LibretroWrapper::~LibretroWrapper() {
    Unload();
}

bool LibretroWrapper::LoadCore(const std::string& core_path) {
    core_handle = dlopen(core_path.c_str(), RTLD_LAZY);
    if (!core_handle) {
        return false;
    }

    // Load libretro core functions
    retro_init = reinterpret_cast<void (*)()>(dlsym(core_handle, "retro_init"));
    retro_deinit = reinterpret_cast<void (*)()>(dlsym(core_handle, "retro_deinit"));
    retro_api_version = reinterpret_cast<unsigned (*)()>(dlsym(core_handle, "retro_api_version"));
    retro_get_system_info = reinterpret_cast<void (*)(struct retro_system_info*)>(dlsym(core_handle, "retro_get_system_info"));
    retro_get_system_av_info = reinterpret_cast<void (*)(struct retro_system_av_info*)>(dlsym(core_handle, "retro_get_system_av_info"));
    retro_set_environment = reinterpret_cast<void (*)(retro_environment_t)>(dlsym(core_handle, "retro_set_environment"));
    retro_set_video_refresh = reinterpret_cast<void (*)(retro_video_refresh_t)>(dlsym(core_handle, "retro_set_video_refresh"));
    retro_set_audio_sample = reinterpret_cast<void (*)(retro_audio_sample_t)>(dlsym(core_handle, "retro_set_audio_sample"));
    retro_set_audio_sample_batch = reinterpret_cast<void (*)(retro_audio_sample_batch_t)>(dlsym(core_handle, "retro_set_audio_sample_batch"));
    retro_set_input_poll = reinterpret_cast<void (*)(retro_input_poll_t)>(dlsym(core_handle, "retro_set_input_poll"));
    retro_set_input_state = reinterpret_cast<void (*)(retro_input_state_t)>(dlsym(core_handle, "retro_set_input_state"));
    retro_set_controller_port_device = reinterpret_cast<void (*)(unsigned, unsigned)>(dlsym(core_handle, "retro_set_controller_port_device"));
    retro_reset = reinterpret_cast<void (*)()>(dlsym(core_handle, "retro_reset"));
    retro_run = reinterpret_cast<void (*)()>(dlsym(core_handle, "retro_run"));
    retro_serialize_size = reinterpret_cast<size_t (*)()>(dlsym(core_handle, "retro_serialize_size"));
    retro_serialize = reinterpret_cast<bool (*)(void*, size_t)>(dlsym(core_handle, "retro_serialize"));
    retro_unserialize = reinterpret_cast<bool (*)(const void*, size_t)>(dlsym(core_handle, "retro_unserialize"));
    retro_load_game = reinterpret_cast<bool (*)(const struct retro_game_info*)>(dlsym(core_handle, "retro_load_game"));
    retro_unload_game = reinterpret_cast<void (*)()>(dlsym(core_handle, "retro_unload_game"));

    if (!retro_init || !retro_deinit || !retro_api_version || !retro_get_system_info ||
        !retro_get_system_av_info || !retro_set_environment || !retro_set_video_refresh ||
        !retro_set_audio_sample || !retro_set_audio_sample_batch || !retro_set_input_poll ||
        !retro_set_input_state || !retro_set_controller_port_device || !retro_reset ||
        !retro_run || !retro_serialize_size || !retro_serialize || !retro_unserialize ||
        !retro_load_game || !retro_unload_game) {
        Unload();
        return false;
    }

    retro_init();
    return true;
}

bool LibretroWrapper::LoadGame(const std::string& game_path) {
    if (!core_handle) {
        return false;
    }

    game_info.path = game_path.c_str();
    game_info.data = nullptr;
    game_info.size = 0;
    game_info.meta = nullptr;

    return retro_load_game(&game_info);
}

void LibretroWrapper::Run() {
    if (core_handle) {
        retro_run();
    }
}

void LibretroWrapper::Reset() {
    if (core_handle) {
        retro_reset();
    }
}

void LibretroWrapper::Unload() {
    if (core_handle) {
        retro_unload_game();
        retro_deinit();
        dlclose(core_handle);
        core_handle = nullptr;
    }
}

} // namespace Core