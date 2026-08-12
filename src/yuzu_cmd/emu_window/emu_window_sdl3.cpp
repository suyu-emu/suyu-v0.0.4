// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SDL3/SDL.h>
#include <SDL3/SDL_timer.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "common/logging.h"
#include "common/scm_rev.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/perf_stats.h"
#include "hid_core/hid_core.h"
#include "input_common/drivers/keyboard.h"
#include "input_common/drivers/mouse.h"
#include "input_common/drivers/touch_screen.h"
#include "input_common/main.h"
#include "yuzu_cmd/emu_window/emu_window_sdl3.h"
#include "yuzu_cmd/yuzu_icon.h"

EmuWindow_SDL3::EmuWindow_SDL3(InputCommon::InputSubsystem* input_subsystem_, Core::System& system_)
    : input_subsystem{input_subsystem_}, system{system_} {
    input_subsystem->Initialize();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) {
        LOG_CRITICAL(Frontend, "Failed to initialize SDL3: {}, Exiting...", SDL_GetError());
        exit(1);
    }
    titlebar_timer = SDL_AddTimer(2000, [](void *userdata, SDL_TimerID, Uint32) -> Uint32 {
        auto* this_ = (EmuWindow_SDL3*)userdata;
        auto const results = this_->system.GetAndResetPerfStats();
        auto const title = fmt::format("{} | {}-{} | FPS: {:.0f} ({:.0f}%)", Common::g_build_fullname, Common::g_scm_branch, Common::g_scm_desc, results.average_game_fps, results.emulation_speed * 100.0f);
        SDL_SetWindowTitle(this_->render_window, title.c_str());
        return 2000;
    }, this);
}

EmuWindow_SDL3::~EmuWindow_SDL3() {
    SDL_RemoveTimer(titlebar_timer);
    system.HIDCore().UnloadInputDevices();
    input_subsystem->Shutdown();
    SDL_Quit();
}

InputCommon::MouseButton EmuWindow_SDL3::SDLButtonToMouseButton(u32 button) const {
    switch (button) {
    case SDL_BUTTON_LEFT:
        return InputCommon::MouseButton::Left;
    case SDL_BUTTON_RIGHT:
        return InputCommon::MouseButton::Right;
    case SDL_BUTTON_MIDDLE:
        return InputCommon::MouseButton::Wheel;
    case SDL_BUTTON_X1:
        return InputCommon::MouseButton::Backward;
    case SDL_BUTTON_X2:
        return InputCommon::MouseButton::Forward;
    default:
        return InputCommon::MouseButton::Undefined;
    }
}

/// @brief Translates pixel position to float position
EmuWindow_SDL3::FloatPairNonHFA EmuWindow_SDL3::MouseToTouchPos(s32 touch_x, s32 touch_y) const {
    int w = 0, h = 0;
    SDL_GetWindowSize(render_window, &w, &h);
    const float fx = float(touch_x) / w;
    const float fy = float(touch_y) / h;
    return {
        std::clamp<float>(fx, 0.0f, 1.0f),
        std::clamp<float>(fy, 0.0f, 1.0f),
        0
    };
}

void EmuWindow_SDL3::OnMouseButton(u32 button, u8 state, s32 x, s32 y) {
    const auto mouse_button = SDLButtonToMouseButton(button);
    if (state != 0) {
        auto const [touch_x, touch_y, _] = MouseToTouchPos(x, y);
        input_subsystem->GetMouse()->PressButton(x, y, mouse_button);
        input_subsystem->GetMouse()->PressMouseButton(mouse_button);
        input_subsystem->GetMouse()->PressTouchButton(touch_x, touch_y, mouse_button);
    } else {
        input_subsystem->GetMouse()->ReleaseButton(mouse_button);
    }
    input_subsystem->GetMouse()->NotifyChanged();
}

void EmuWindow_SDL3::OnMouseMotion(s32 x, s32 y) {
    auto const [touch_x, touch_y, _] = MouseToTouchPos(x, y);
    input_subsystem->GetMouse()->Move(x, y, 0, 0);
    input_subsystem->GetMouse()->MouseMove(touch_x, touch_y);
    input_subsystem->GetMouse()->TouchMove(touch_x, touch_y);
    input_subsystem->GetMouse()->NotifyChanged();
}

void EmuWindow_SDL3::OnFingerDown(float x, float y, std::size_t id) {
    input_subsystem->GetTouchScreen()->TouchPressed(x, y, id);
}

void EmuWindow_SDL3::OnFingerMotion(float x, float y, std::size_t id) {
    input_subsystem->GetTouchScreen()->TouchMoved(x, y, id);
}

void EmuWindow_SDL3::OnFingerUp() {
    input_subsystem->GetTouchScreen()->ReleaseAllTouch();
}

void EmuWindow_SDL3::OnKeyEvent(int key, u8 state) {
    if (state != 0) {
        input_subsystem->GetKeyboard()->PressKey(static_cast<std::size_t>(key));
    } else {
        input_subsystem->GetKeyboard()->ReleaseKey(static_cast<std::size_t>(key));
    }
}

bool EmuWindow_SDL3::IsOpen() const {
    return is_open;
}

bool EmuWindow_SDL3::IsShown() const {
    return is_shown;
}

void EmuWindow_SDL3::OnResize() {
    int width, height;
    SDL_GetWindowSizeInPixels(render_window, &width, &height);
    UpdateCurrentFramebufferLayout(width, height);
}

void EmuWindow_SDL3::ShowCursor(bool show_cursor) {
    if (show_cursor) {
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }
}

void EmuWindow_SDL3::Fullscreen() {
    SDL_DisplayMode display_mode;
    switch (Settings::values.fullscreen_mode.GetValue()) {
    case Settings::FullscreenMode::Exclusive:
        // Set window size to render size before entering fullscreen in exclusive mode.
        if (const SDL_DisplayMode* display_mode_ptr = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(render_window))) {
            display_mode = *display_mode_ptr;
            SDL_SetWindowSize(render_window, display_mode.w, display_mode.h);
            SDL_SetWindowFullscreenMode(render_window, &display_mode);
        } else {
            LOG_ERROR(Frontend, "SDL_GetDesktopDisplayMode failed: {}", SDL_GetError());
        }

        if (SDL_SetWindowFullscreen(render_window, true)) {
            return;
        }

        LOG_ERROR(Frontend, "Fullscreening failed: {}", SDL_GetError());
        LOG_INFO(Frontend, "Attempting to use borderless fullscreen...");
        [[fallthrough]];
    case Settings::FullscreenMode::Borderless:
        SDL_SetWindowFullscreenMode(render_window, nullptr);
        if (SDL_SetWindowFullscreen(render_window, true)) {
            return;
        }

        LOG_ERROR(Frontend, "Borderless fullscreening failed: {}", SDL_GetError());
        [[fallthrough]];
    default:
        // Fallback algorithm: Maximise window.
        // Works on all systems (unless something is seriously wrong), so no fallback for this one.
        LOG_INFO(Frontend, "Falling back on a maximised window...");
        SDL_MaximizeWindow(render_window);
        break;
    }
}

void EmuWindow_SDL3::OnEvent(SDL_Event& event) {
    // Notice how we skip the "update title" aspect on most events
    // this is because some WMs do NOT like changing titles while resizing
    // so let's just... not do that, thanks :)
    // Afterall we don't really expect the user to pay attention to the titlebar
    // while they're moving a lot of shit around...
    switch (event.type) {
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
        return OnResize();
    case SDL_EVENT_WINDOW_MINIMIZED:
        is_shown = false;
        return OnResize();
    case SDL_EVENT_WINDOW_EXPOSED:
        is_shown = true;
        return OnResize();
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        is_open = false;
        return;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        return OnKeyEvent(int(event.key.scancode), event.key.down ? 1 : 0);
    case SDL_EVENT_MOUSE_MOTION:
        // ignore if it came from touch
        if (event.button.which != SDL_TOUCH_MOUSEID)
            OnMouseMotion(event.motion.x, event.motion.y);
        return;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        // ignore if it came from touch
        if (event.button.which != SDL_TOUCH_MOUSEID)
            OnMouseButton(event.button.button, event.button.down ? 1 : 0, s32(event.button.x), s32(event.button.y));
        return;
    case SDL_EVENT_FINGER_DOWN:
        return OnFingerDown(event.tfinger.x, event.tfinger.y, std::size_t(event.tfinger.touchID));
    case SDL_EVENT_FINGER_MOTION:
        return OnFingerMotion(event.tfinger.x, event.tfinger.y, std::size_t(event.tfinger.touchID));
    case SDL_EVENT_FINGER_UP:
        return OnFingerUp();
    case SDL_EVENT_QUIT:
        is_open = false;
        return;
    default:
        break;
    }
}

// Credits to Samantas5855 and others for this function.
void EmuWindow_SDL3::SetWindowIcon() {
#if defined(__EMSCRIPTEN__) || defined(__wasi__)
    // Icons do not work yet
#else
    SDL_IOStream* const yuzu_icon_stream = SDL_IOFromConstMem((void*)yuzu_icon, yuzu_icon_size);
    if (yuzu_icon_stream == nullptr) {
        LOG_WARNING(Frontend, "Failed to create Eden icon stream.");
        return;
    }
    SDL_Surface* const window_icon = SDL_LoadBMP_IO(yuzu_icon_stream, true);
    if (window_icon == nullptr) {
        LOG_WARNING(Frontend, "Failed to read BMP from stream.");
        return;
    }
    // The icon is attached to the window pointer
    SDL_SetWindowIcon(render_window, window_icon);
    SDL_DestroySurface(window_icon);
#endif
}

void EmuWindow_SDL3::OnMinimalClientAreaChangeRequest(std::pair<u32, u32> minimal_size) {
    SDL_SetWindowMinimumSize(render_window, minimal_size.first, minimal_size.second);
}
