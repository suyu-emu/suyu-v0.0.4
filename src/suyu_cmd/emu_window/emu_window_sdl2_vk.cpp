// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <memory>
#include <string>

#include <fmt/format.h>

#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "suyu_cmd/emu_window/emu_window_sdl2_vk.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"

#include <SDL3/SDL.h>

EmuWindow_SDL2_VK::EmuWindow_SDL2_VK(InputCommon::InputSubsystem* input_subsystem_,
                                     Core::System& system_, bool fullscreen)
    : EmuWindow_SDL2{input_subsystem_, system_} {
    const std::string window_title = fmt::format("suyu {} | {}-{} (Vulkan)", Common::g_build_name,
                                                 Common::g_scm_branch, Common::g_scm_desc);
    render_window =
        SDL_CreateWindow(window_title.c_str(),
                         Layout::ScreenUndocked::Width, Layout::ScreenUndocked::Height,
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

    if (render_window == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL3 window: {}", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }

    SetWindowIcon();

    if (fullscreen) {
        Fullscreen();
        ShowCursor(false);
    }

    // Retrieve native window handles via SDL3 property system
    SDL_PropertiesID props = SDL_GetWindowProperties(render_window);

#if defined(SDL_PLATFORM_WIN32)
    window_info.type = Core::Frontend::WindowSystemType::Windows;
    window_info.render_surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    if (!window_info.render_surface) {
        LOG_CRITICAL(Frontend, "Failed to get Win32 HWND from window properties: {}", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }
#elif defined(SDL_PLATFORM_LINUX) || defined(SDL_PLATFORM_FREEBSD)
    {
        const char* driver = SDL_GetCurrentVideoDriver();
        if (driver && SDL_strcmp(driver, "x11") == 0) {
            window_info.type = Core::Frontend::WindowSystemType::X11;
            window_info.display_connection = SDL_GetPointerProperty(
                props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
            window_info.render_surface = reinterpret_cast<void*>(
                SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
        } else if (driver && SDL_strcmp(driver, "wayland") == 0) {
            window_info.type = Core::Frontend::WindowSystemType::Wayland;
            window_info.display_connection = SDL_GetPointerProperty(
                props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
            window_info.render_surface = SDL_GetPointerProperty(
                props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
        } else {
            LOG_CRITICAL(Frontend, "Unsupported video driver: {}", driver ? driver : "(null)");
            std::exit(EXIT_FAILURE);
        }
    }
#elif defined(SDL_PLATFORM_MACOS)
    window_info.type = Core::Frontend::WindowSystemType::Cocoa;
    window_info.render_surface = SDL_Metal_CreateView(render_window);
#elif defined(SDL_PLATFORM_ANDROID)
    window_info.type = Core::Frontend::WindowSystemType::Android;
    window_info.render_surface = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr);
#else
    LOG_CRITICAL(Frontend, "Window manager subsystem not implemented for this platform");
    std::exit(EXIT_FAILURE);
#endif

    SDL_ShowWindow(render_window);
    OnResize();
    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);
    SDL_PumpEvents();
    LOG_INFO(Frontend, "suyu Version: {} | {}-{} (Vulkan)", Common::g_build_name,
             Common::g_scm_branch, Common::g_scm_desc);
}

EmuWindow_SDL2_VK::~EmuWindow_SDL2_VK() = default;

std::unique_ptr<Core::Frontend::GraphicsContext> EmuWindow_SDL2_VK::CreateSharedContext() const {
    return std::make_unique<DummyContext>();
}
