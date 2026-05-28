#include "core/libretro_wrapper.h"

#include "nintendo_library/nintendo_library.h"

#include "common/logging/log.h"

namespace Core {

LibretroWrapper::LibretroWrapper() : nintendo_library(std::make_unique<Nintendo::Library>()) {}

LibretroWrapper::~LibretroWrapper() {
    Unload();
}

bool LibretroWrapper::LoadCore(const std::string& core_path) {
    Unload();

    if (!core_library.Open(core_path.c_str())) {
        LOG_ERROR(Core, "Failed to load libretro core: {}", core_path);
        return false;
    }

    // Load libretro core functions
#define LOAD_SYMBOL(S)                                                                          \
    if (!core_library.GetSymbol(#S, &S)) {                                                     \
        LOG_ERROR(Core, "Failed to load libretro symbol {} from {}", #S, core_path);           \
        Unload();                                                                               \
        return false;                                                                           \
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

    retro_init();
    LOG_INFO(Core, "Libretro core loaded successfully: {}", core_path);
    return true;
}

bool LibretroWrapper::LoadGame(const std::string& game_path) {
    if (!core_library.IsOpen()) {
        LOG_ERROR(Core, "Libretro core not loaded");
        return false;
    }

    if (game_loaded && retro_unload_game) {
        retro_unload_game();
        game_loaded = false;
    }

    loaded_game_path = game_path;
    game_info.path = loaded_game_path.c_str();
    game_info.data = nullptr;
    game_info.size = 0;
    game_info.meta = nullptr;

    if (!retro_load_game(&game_info)) {
        LOG_ERROR(Core, "Failed to load game through libretro: {}", game_path);
        return false;
    }

    game_loaded = true;
    LOG_INFO(Core, "Game loaded successfully: {}", game_path);
    return true;
}

void LibretroWrapper::Run() {
    if (core_library.IsOpen()) {
        retro_run();
    } else {
        LOG_WARNING(Core, "Cannot run libretro core: no core is loaded");
    }
}

void LibretroWrapper::Reset() {
    if (core_library.IsOpen()) {
        retro_reset();
    } else {
        LOG_WARNING(Core, "Cannot reset libretro core: no core is loaded");
    }
}

void LibretroWrapper::Unload() {
    if (game_loaded && retro_unload_game) {
        retro_unload_game();
        game_loaded = false;
    }

    if (core_library.IsOpen()) {
        if (retro_deinit) {
            retro_deinit();
        }
        core_library.Close();
    }
    loaded_game_path.clear();

    retro_init = nullptr;
    retro_deinit = nullptr;
    retro_api_version = nullptr;
    retro_get_system_info = nullptr;
    retro_get_system_av_info = nullptr;
    retro_set_environment = nullptr;
    retro_set_video_refresh = nullptr;
    retro_set_audio_sample = nullptr;
    retro_set_audio_sample_batch = nullptr;
    retro_set_input_poll = nullptr;
    retro_set_input_state = nullptr;
    retro_set_controller_port_device = nullptr;
    retro_reset = nullptr;
    retro_run = nullptr;
    retro_serialize_size = nullptr;
    retro_serialize = nullptr;
    retro_unserialize = nullptr;
    retro_load_game = nullptr;
    retro_unload_game = nullptr;

    if (nintendo_library && nintendo_library->IsInitialized()) {
        nintendo_library->Shutdown();
    }
}

bool LibretroWrapper::InitializeNintendoLibrary() {
    if (!nintendo_library) {
        nintendo_library = std::make_unique<Nintendo::Library>();
    }

    if (nintendo_library->IsInitialized()) {
        return true;
    }

    return nintendo_library->Initialize();
}

bool LibretroWrapper::AuthenticateNintendoAccount(const std::string& username,
                                                  const std::string& password) {
    if (!InitializeNintendoLibrary()) {
        return false;
    }

    return nintendo_library->StartAuthentication(username, password);
}

std::vector<std::string> LibretroWrapper::GetNintendoGameTitles() {
    std::vector<std::string> titles;
    if (!nintendo_library || !nintendo_library->IsInitialized()) {
        return titles;
    }

    for (const auto& game : nintendo_library->GetGameList()) {
        if (!game.title_name.empty()) {
            titles.push_back(game.title_name);
        } else if (!game.title_id.empty()) {
            titles.push_back(game.title_id);
        }
    }

    return titles;
}

} // namespace Core
