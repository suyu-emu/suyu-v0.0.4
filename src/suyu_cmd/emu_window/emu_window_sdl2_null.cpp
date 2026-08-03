// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <memory>
#include <string>

#include <fmt/format.h>

#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "suyu_cmd/emu_window/emu_window_sdl2_null.h"
#include "video_core/renderer_null/renderer_null.h"

#include <SDL3/SDL.h>

EmuWindow_SDL2_Null::EmuWindow_SDL2_Null(InputCommon::InputSubsystem* input_subsystem_,
                                         Core::System& system_, bool fullscreen)
    : EmuWindow_SDL2{input_subsystem_, system_} {
    const std::string window_title = fmt::format("suyu {} | {}-{} (Null)", Common::g_build_name,
                                                 Common::g_scm_branch, Common::g_scm_desc);
    render_window =
        SDL_CreateWindow(window_title.c_str(),
                         Layout::ScreenUndocked::Width, Layout::ScreenUndocked::Height,
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

    SetWindowIcon();

    if (fullscreen) {
        Fullscreen();
        ShowCursor(false);
    }

    OnResize();
    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);
    SDL_PumpEvents();
    LOG_INFO(Frontend, "suyu Version: {} | {}-{} (Null)", Common::g_build_name,
             Common::g_scm_branch, Common::g_scm_desc);
}

EmuWindow_SDL2_Null::~EmuWindow_SDL2_Null() = default;

std::unique_ptr<Core::Frontend::GraphicsContext> EmuWindow_SDL2_Null::CreateSharedContext() const {
    return std::make_unique<DummyContext>();
}
