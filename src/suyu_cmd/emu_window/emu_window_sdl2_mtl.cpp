// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <memory>
#include <string>

#include <fmt/format.h>

#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "suyu_cmd/emu_window/emu_window_sdl2_mtl.h"
#include "video_core/renderer_metal/renderer_metal.h"

#include <SDL3/SDL.h>

EmuWindow_SDL2_MTL::EmuWindow_SDL2_MTL(InputCommon::InputSubsystem* input_subsystem_,
                                       Core::System& system_, bool fullscreen)
    : EmuWindow_SDL2{input_subsystem_, system_} {
    const std::string window_title = fmt::format("suyu {} | {}-{} (Metal)", Common::g_build_name,
                                                 Common::g_scm_branch, Common::g_scm_desc);
    render_window =
        SDL_CreateWindow(window_title.c_str(),
                         Layout::ScreenUndocked::Width, Layout::ScreenUndocked::Height,
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_METAL);

    if (render_window == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL3 window: {}", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }

    SetWindowIcon();

    if (fullscreen) {
        Fullscreen();
        ShowCursor(false);
    }

    window_info.type = Core::Frontend::WindowSystemType::Cocoa;
    window_info.render_surface = SDL_Metal_CreateView(render_window);

    OnResize();
    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);
    SDL_PumpEvents();
    LOG_INFO(Frontend, "suyu Version: {} | {}-{} (Metal)", Common::g_build_name,
             Common::g_scm_branch, Common::g_scm_desc);
}

EmuWindow_SDL2_MTL::~EmuWindow_SDL2_MTL() = default;

std::unique_ptr<Core::Frontend::GraphicsContext> EmuWindow_SDL2_MTL::CreateSharedContext() const {
    return std::make_unique<DummyContext>();
}
