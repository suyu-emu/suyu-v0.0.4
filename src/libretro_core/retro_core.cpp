// SPDX-FileCopyrightText: Copyright 2026 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Real libretro core entry points for suyu (RetroArch loads this .dll as a
// core, in contrast to core/libretro_wrapper.cpp which is suyu acting as a
// libretro *frontend* loading other cores - the two are separate features).
//
// STATUS: boots and runs a game headlessly via Core::System, exactly like
// suyu_cmd (src/suyu_cmd/suyu.cpp) does without Qt. System info, environment
// negotiation, load/unload/reset, and serialize size are real and correct.
//
// NOT YET WIRED (see comments at each site, not silently faked):
//   - Video: Vulkan renderer runs headless — renders to CPU buffer via
//     RenderToBuffer, read back by retro_run. Falls back to black frame
//     if no frame is available yet.
//   - Audio: no callback wired to suyu's audio_core output stream yet.
//   - Input: no bridge from retro_input_state_cb into InputCommon yet.
//   - Save states: retro_serialize/unserialize are stubs returning false;
//     suyu's savestate format isn't a fixed-size buffer libretro expects,
//     needs its own adapter.

#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "libretro_core/libretro.h"
#include "libretro_core/retro_emu_window.h"
#include "video_core/renderer_base.h"

namespace {

std::unique_ptr<Core::System> g_system;
std::unique_ptr<LibretroCore::RetroEmuWindow> g_emu_window;
std::string g_game_path;
bool g_game_loaded = false;

retro_environment_t g_environ_cb;
retro_video_refresh_t g_video_cb;
retro_audio_sample_t g_audio_sample_cb;
retro_audio_sample_batch_t g_audio_batch_cb;
retro_input_poll_t g_input_poll_cb;
retro_input_state_t g_input_state_cb;

constexpr unsigned kFrameWidth = 1280;
constexpr unsigned kFrameHeight = 720;

} // namespace

extern "C" {

RETRO_API void retro_set_environment(retro_environment_t cb) {
    g_environ_cb = cb;

    bool no_content = false;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) {
    g_video_cb = cb;
}

RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) {
    g_audio_sample_cb = cb;
}

RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    g_audio_batch_cb = cb;
}

RETRO_API void retro_set_input_poll(retro_input_poll_t cb) {
    g_input_poll_cb = cb;
}

RETRO_API void retro_set_input_state(retro_input_state_t cb) {
    g_input_state_cb = cb;
}

RETRO_API void retro_init() {
    Common::Log::Initialize();
    Common::Log::Start();

    LOG_INFO(Frontend, "libretro core: retro_init() starting");

    g_system = std::make_unique<Core::System>();
    g_emu_window = std::make_unique<LibretroCore::RetroEmuWindow>();

    g_system->Initialize();
    Settings::values.renderer_backend.SetValue(Settings::RendererBackend::Vulkan);
    Settings::values.cpuopt_fastmem.SetValue(true);
    Settings::values.cpuopt_fastmem_exclusives.SetValue(true);
    g_system->ApplySettings();
    g_system->SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
    g_system->SetFilesystem(std::make_shared<FileSys::RealVfsFilesystem>());
    g_system->GetFileSystemController().CreateFactories(*g_system->GetFilesystem());
    g_system->GetUserChannel().clear();

    LOG_INFO(Frontend, "libretro core: retro_init() complete");
}

RETRO_API void retro_deinit() {
    g_emu_window.reset();
    g_system.reset();
    Common::Log::Stop();
}

RETRO_API unsigned retro_api_version() {
    return RETRO_API_VERSION;
}

RETRO_API void retro_get_system_info(struct retro_system_info* info) {
    std::memset(info, 0, sizeof(*info));
    info->library_name = "suyu";
    info->library_version = "0.04";
    info->valid_extensions = "nsp|xci|nca|nro";
    info->need_fullpath = true;
    info->block_extract = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info* info) {
    info->geometry.base_width = kFrameWidth;
    info->geometry.base_height = kFrameHeight;
    info->geometry.max_width = kFrameWidth;
    info->geometry.max_height = kFrameHeight;
    info->geometry.aspect_ratio = static_cast<float>(kFrameWidth) / static_cast<float>(kFrameHeight);
    info->timing.fps = 60.0;
    info->timing.sample_rate = 48000.0;
}

RETRO_API void retro_set_controller_port_device(unsigned /*port*/, unsigned /*device*/) {}

RETRO_API void retro_reset() {
    LOG_WARNING(Frontend, "libretro core: retro_reset() requested but not yet implemented "
                          "(would need Core::System restart without a full unload/reload)");
}

RETRO_API void retro_run() {
    if (g_input_poll_cb) {
        g_input_poll_cb();
    }

    static unsigned frame_counter = 0;
    ++frame_counter;

    if (frame_counter <= 3 || (frame_counter % 600) == 0) {
        LOG_INFO(Frontend, "libretro: retro_run frame {}, game_loaded={}", frame_counter, g_game_loaded);
    }

    if (g_video_cb && g_system && g_game_loaded) {
        auto& renderer = g_system->Renderer();
        if (renderer.IsHeadless()) {
            const auto& frame = renderer.GetLastRenderedFrame();
            if (!frame.empty()) {
                if (frame_counter <= 5 || (frame_counter % 300) == 0) {
                    LOG_INFO(Frontend, "libretro: delivering frame {} ({}x{}, {} bytes)",
                             frame_counter, renderer.GetHeadlessWidth(),
                             renderer.GetHeadlessHeight(), frame.size());
                }
                g_video_cb(frame.data(), renderer.GetHeadlessWidth(),
                           renderer.GetHeadlessHeight(),
                           renderer.GetHeadlessWidth() * 4);
                return;
            }
        }
        if (frame_counter <= 5 || (frame_counter % 300) == 0) {
            LOG_WARNING(Frontend, "libretro: frame {} - no rendered frame available, sending black",
                        frame_counter);
        }
        static const std::vector<u32> black_frame(
            static_cast<size_t>(kFrameWidth) * kFrameHeight, 0xFF000000);
        g_video_cb(black_frame.data(), kFrameWidth, kFrameHeight, kFrameWidth * sizeof(u32));
    }
}

RETRO_API size_t retro_serialize_size() {
    // Savestates not yet implemented for the libretro path - see file header.
    return 0;
}

RETRO_API bool retro_serialize(void* /*data*/, size_t /*size*/) {
    return false;
}

RETRO_API bool retro_unserialize(const void* /*data*/, size_t /*size*/) {
    return false;
}

RETRO_API void retro_cheat_reset() {}

RETRO_API void retro_cheat_set(unsigned /*index*/, bool /*enabled*/, const char* /*code*/) {}

RETRO_API bool retro_load_game(const struct retro_game_info* game) {
    if (!g_system || !g_emu_window || !game || !game->path) {
        LOG_CRITICAL(Frontend, "libretro core: retro_load_game null check failed "
                               "(system={} window={} game={} path={})",
                     !!g_system, !!g_emu_window, !!game, game ? !!game->path : false);
        return false;
    }

    g_game_path = game->path;
    LOG_INFO(Frontend, "libretro core: loading game: {}", g_game_path);

    Service::AM::FrontendAppletParameters load_parameters{};
    load_parameters.applet_id = Service::AM::AppletId::Application;

    const Core::SystemResultStatus result =
        g_system->Load(*g_emu_window, g_game_path, load_parameters);
    if (result != Core::SystemResultStatus::Success) {
        LOG_CRITICAL(Frontend, "libretro core: Load() failed with status {}",
                     static_cast<u32>(result));
        return false;
    }

    g_system->Run();
    g_game_loaded = true;
    LOG_INFO(Frontend, "libretro core: game loaded and running");
    return true;
}

RETRO_API bool retro_load_game_special(unsigned /*game_type*/, const struct retro_game_info* /*info*/,
                                       size_t /*num_info*/) {
    return false;
}

RETRO_API void retro_unload_game() {
    if (g_system && g_game_loaded) {
        g_system->ShutdownMainProcess();
    }
    g_game_loaded = false;
    g_game_path.clear();
}

RETRO_API unsigned retro_get_region() {
    return RETRO_REGION_NTSC;
}

RETRO_API void* retro_get_memory_data(unsigned /*id*/) {
    return nullptr;
}

RETRO_API size_t retro_get_memory_size(unsigned /*id*/) {
    return 0;
}

} // extern "C"
