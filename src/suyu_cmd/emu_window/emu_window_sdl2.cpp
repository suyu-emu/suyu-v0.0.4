// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SDL3/SDL.h>
// SDL3 removed these constants; define compat shims
static constexpr Uint8 SDL_PRESSED = 1;
static constexpr Uint8 SDL_RELEASED = 0;

#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/perf_stats.h"
#include "hid_core/hid_core.h"
#include "input_common/drivers/keyboard.h"
#include "input_common/drivers/mouse.h"
#include "input_common/drivers/touch_screen.h"
#include "input_common/main.h"
#include "common/param_package.h"
#include "suyu_cmd/emu_window/emu_window_sdl2.h"
#include "suyu_cmd/suyu_icon.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <array>
#include <filesystem>
#include <iterator>
#include <vector>
#include <system_error>
#include <string>
#include <vector>

// ── F12 debug panel ─────────────────────────────────────────────────────────
// A real interactive window (not a message box): live status text refreshed on
// a timer, a list of the mod folders currently visible to the patch manager,
// and buttons that open the folders this build actually uses. Deliberately
// carries no emulator branding — an exported game shows the game's own name.
namespace {

constexpr int kIdStatus = 1001;
constexpr int kIdMods = 1002;
constexpr int kIdOpenUser = 1003;
constexpr int kIdOpenMods = 1004;
constexpr int kIdOpenKeys = 1005;
constexpr int kIdClose = 1006;
constexpr int kIdDevices = 1007;
constexpr int kIdApplyPad = 1008;
constexpr int kIdKeyboard = 1009;
constexpr int kIdRescanPads = 1010;
constexpr UINT_PTR kTimer = 1;

std::filesystem::path DevExeDir() {
    wchar_t exe_path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    return std::filesystem::path(exe_path).parent_path();
}

std::wstring DevKeysDir() {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::KeysDir).wstring();
}

void DevOpen(const std::filesystem::path& p) {
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    ShellExecuteW(nullptr, L"explore", p.wstring().c_str(), nullptr, nullptr, SW_SHOW);
}

struct DevPanelState {
    Core::System* system{};
    InputCommon::InputSubsystem* input{};
    HWND status{};
    HWND mods{};
    HWND devices{};
    std::vector<Common::ParamPackage> device_list;
};

// Controller setup, done the way a player expects: pick the pad from a list and
// press one button. The full per-button remapper belongs in the emulator's own
// UI - what a shipped game build needs is for a plugged-in pad to just work,
// and a way back to keyboard when it does not.
void DevRefreshDevices(DevPanelState& st) {
    SendMessageW(st.devices, CB_RESETCONTENT, 0, 0);
    st.device_list.clear();
    if (st.input == nullptr) {
        return;
    }
    for (const auto& device : st.input->GetInputDevices()) {
        const std::string name = device.Get("display", device.Get("class", "Unknown"));
        if (name == "Any" || name == "Keyboard/Mouse") {
            continue;
        }
        st.device_list.push_back(device);
        const std::wstring wide(name.begin(), name.end());
        SendMessageW(st.devices, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
    }
    if (st.device_list.empty()) {
        SendMessageW(st.devices, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(L"(no controller detected - plug one in and rescan)"));
    }
    SendMessageW(st.devices, CB_SETCURSEL, 0, 0);
}

void DevApplyPadMapping(DevPanelState& st) {
    const auto index = static_cast<std::size_t>(SendMessageW(st.devices, CB_GETCURSEL, 0, 0));
    if (st.input == nullptr || index >= st.device_list.size()) {
        MessageBoxW(nullptr, L"No controller selected.", L"Controls", MB_OK | MB_ICONINFORMATION);
        return;
    }
    const auto& device = st.device_list[index];
    // GetValue() hands back a reference to the live array, so the mappings are
    // written straight into the setting.
    auto& player = Settings::values.players.GetValue()[0];
    for (const auto& [button, param] : st.input->GetButtonMappingForDevice(device)) {
        player.buttons[button] = param.Serialize();
    }
    for (const auto& [analog, param] : st.input->GetAnalogMappingForDevice(device)) {
        player.analogs[analog] = param.Serialize();
    }
    for (const auto& [motion, param] : st.input->GetMotionMappingForDevice(device)) {
        player.motions[motion] = param.Serialize();
    }
    player.connected = true;
    if (st.system != nullptr) {
        st.system->HIDCore().ReloadInputDevices();
    }
    MessageBoxW(nullptr, L"Controller mapped to Player 1.", L"Controls",
                MB_OK | MB_ICONINFORMATION);
}

void DevApplyKeyboardMapping(DevPanelState& st) {
    // Same layout the emulator ships as its keyboard default.
    static constexpr std::array<int, Settings::NativeButton::NumButtons> kButtons = {
        SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_Z, SDL_SCANCODE_X,
        SDL_SCANCODE_T, SDL_SCANCODE_G, SDL_SCANCODE_F, SDL_SCANCODE_H,
        SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_M, SDL_SCANCODE_N,
        SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_B,
    };
    static constexpr std::array<std::array<int, 4>, Settings::NativeAnalog::NumAnalogs> kAnalogs{{
        {SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT},
        {SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L},
    }};
    // GetValue() hands back a reference to the live array, so the mappings are
    // written straight into the setting.
    auto& player = Settings::values.players.GetValue()[0];
    for (std::size_t i = 0; i < kButtons.size() && i < player.buttons.size(); ++i) {
        player.buttons[i] = InputCommon::GenerateKeyboardParam(kButtons[i]);
    }
    for (std::size_t i = 0; i < kAnalogs.size() && i < player.analogs.size(); ++i) {
        player.analogs[i] = InputCommon::GenerateAnalogParamFromKeys(
            kAnalogs[i][0], kAnalogs[i][1], kAnalogs[i][2], kAnalogs[i][3], 0, 0.5f);
    }
    player.connected = true;
    if (st.system != nullptr) {
        st.system->HIDCore().ReloadInputDevices();
    }
    MessageBoxW(nullptr, L"Keyboard controls restored for Player 1.", L"Controls",
                MB_OK | MB_ICONINFORMATION);
}

std::wstring DevStatusText(Core::System& system) {
    std::string game_name;
    [[maybe_unused]] auto _ = system.GetGameName(game_name);
    const auto perf = system.GetAndResetPerfStats();
    wchar_t buf[2048];
    const auto exe_dir = DevExeDir();
    swprintf(buf, std::size(buf),
             L"Title:        %hs\r\n"
             L"Title ID:     %016llX\r\n"
             L"FPS:          %.1f   Speed: %.0f%%   Frame: %.2f ms\r\n"
             L"CPU backend:  %hs\r\n"
             L"\r\n"
             L"User data:    %s\r\n"
             L"Mods:         %s\r\n"
             L"Keys:         %s\r\n",
             game_name.empty() ? "(not loaded)" : game_name.c_str(),
             static_cast<unsigned long long>(system.GetApplicationProcessProgramID()),
             perf.average_game_fps, perf.emulation_speed * 100.0, perf.frametime * 1000.0,
             g_native_export_mode ? "ArmRecomp (static recompiled modules)" : "dynarmic JIT",
             (exe_dir / L"user").wstring().c_str(), (exe_dir / L"mods").wstring().c_str(),
             DevKeysDir().c_str());
    return buf;
}

void DevRefreshMods(HWND list) {
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    const auto mods_dir = DevExeDir() / L"mods";
    bool any = false;
    std::error_code ec;
    if (std::filesystem::is_directory(mods_dir, ec)) {
        for (const auto& tid : std::filesystem::directory_iterator(mods_dir, ec)) {
            if (!tid.is_directory()) {
                continue;
            }
            for (const auto& mod : std::filesystem::directory_iterator(tid.path(), ec)) {
                const std::wstring entry =
                    tid.path().filename().wstring() + L"  /  " + mod.path().filename().wstring();
                SendMessageW(list, LB_ADDSTRING, 0,
                             reinterpret_cast<LPARAM>(entry.c_str()));
                any = true;
            }
        }
    }
    if (!any) {
        SendMessageW(list, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(L"(none - drop <title id>/<mod name>/ into mods/)"));
    }
}

LRESULT CALLBACK DevPanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<DevPanelState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_TIMER:
        if (st != nullptr && st->system != nullptr) {
            SetWindowTextW(st->status, DevStatusText(*st->system).c_str());
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case kIdOpenUser:
            DevOpen(DevExeDir() / L"user");
            return 0;
        case kIdOpenMods:
            DevOpen(DevExeDir() / L"mods");
            return 0;
        case kIdOpenKeys:
            DevOpen(DevKeysDir());
            return 0;
        case kIdClose:
            DestroyWindow(hwnd);
            return 0;
        case kIdRescanPads:
            if (st != nullptr) {
                DevRefreshDevices(*st);
            }
            return 0;
        case kIdApplyPad:
            if (st != nullptr) {
                DevApplyPadMapping(*st);
            }
            return 0;
        case kIdKeyboard:
            if (st != nullptr) {
                DevApplyKeyboardMapping(*st);
            }
            return 0;
        case kIdMods:
            if (HIWORD(wp) == LBN_DBLCLK && st != nullptr) {
                DevRefreshMods(st->mods);
            }
            return 0;
        default:
            break;
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kTimer);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ShowDevMenu(Core::System& system, InputCommon::InputSubsystem* input) {
    static bool registered = false;
    static const wchar_t* kClass = L"SuyuGameDebugPanel";
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DevPanelProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = kClass;
        wc.hIcon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                                 IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
        RegisterClassExW(&wc);
        registered = true;
    }

    std::string game_name;
    [[maybe_unused]] auto _ = system.GetGameName(game_name);
    const std::wstring title =
        (game_name.empty() ? std::wstring(L"Game") : std::wstring(game_name.begin(), game_name.end())) +
        L" - Debug Panel (F12)";

    const HWND hwnd = CreateWindowExW(0, kClass, title.c_str(),
                                      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT,
                                      CW_USEDEFAULT, 720, 560, nullptr, nullptr,
                                      GetModuleHandleW(nullptr), nullptr);
    if (hwnd == nullptr) {
        return;
    }

    const HINSTANCE inst = GetModuleHandleW(nullptr);
    DevPanelState state{};
    state.system = &system;
    state.input = input;
    state.status = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                   WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY |
                                       ES_AUTOVSCROLL | WS_VSCROLL,
                                   10, 10, 690, 190, hwnd,
                                   reinterpret_cast<HMENU>(kIdStatus), inst, nullptr);
    CreateWindowExW(0, L"STATIC", L"Mods discovered under mods/ (double-click to rescan):",
                    WS_CHILD | WS_VISIBLE, 12, 208, 500, 18, hwnd, nullptr, inst, nullptr);
    state.mods = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 10, 228, 690,
                                 110, hwnd, reinterpret_cast<HMENU>(kIdMods), inst, nullptr);
    CreateWindowExW(0, L"STATIC", L"Controls - Player 1:", WS_CHILD | WS_VISIBLE, 12, 348, 140, 18,
                    hwnd, nullptr, inst, nullptr);
    state.devices = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, 150, 344,
                                    280, 200, hwnd, reinterpret_cast<HMENU>(kIdDevices), inst,
                                    nullptr);
    CreateWindowExW(0, L"BUTTON", L"Rescan", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 440, 344, 80,
                    26, hwnd, reinterpret_cast<HMENU>(kIdRescanPads), inst, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Use controller", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 528, 344,
                    172, 26, hwnd, reinterpret_cast<HMENU>(kIdApplyPad), inst, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Use keyboard", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 528, 376,
                    172, 26, hwnd, reinterpret_cast<HMENU>(kIdKeyboard), inst, nullptr);
    const auto button = [&](const wchar_t* text, int x, int id) {
        CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, 466, 160, 28,
                        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
    };
    button(L"Open user data folder", 10, kIdOpenUser);
    button(L"Open mods folder", 180, kIdOpenMods);
    button(L"Open keys folder", 350, kIdOpenKeys);
    button(L"Resume", 540, kIdClose);

    const HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    EnumChildWindows(
        hwnd,
        [](HWND child, LPARAM f) -> BOOL {
            SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(f), TRUE);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(font));

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));
    SetWindowTextW(state.status, DevStatusText(system).c_str());
    DevRefreshMods(state.mods);
    DevRefreshDevices(state);
    SetTimer(hwnd, kTimer, 500, nullptr);
    ShowWindow(hwnd, SW_SHOW);

    // Modal to the game: emulation stays paused-ish while the panel is up, and
    // the panel gets its own pump so the live status keeps refreshing.
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
}

} // namespace
#endif

EmuWindow_SDL2::EmuWindow_SDL2(InputCommon::InputSubsystem* input_subsystem_, Core::System& system_)
    : input_subsystem{input_subsystem_}, system{system_} {
    input_subsystem->Initialize();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD) < 0) {
        LOG_CRITICAL(Frontend, "Failed to initialize SDL3: {}, Exiting...", SDL_GetError());
        exit(1);
    }
}

EmuWindow_SDL2::~EmuWindow_SDL2() {
    system.HIDCore().UnloadInputDevices();
    input_subsystem->Shutdown();
    SDL_Quit();
}

InputCommon::MouseButton EmuWindow_SDL2::SDLButtonToMouseButton(u32 button) const {
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

std::pair<float, float> EmuWindow_SDL2::MouseToTouchPos(s32 touch_x, s32 touch_y) const {
    int w, h;
    SDL_GetWindowSize(render_window, &w, &h);
    const float fx = static_cast<float>(touch_x) / w;
    const float fy = static_cast<float>(touch_y) / h;

    return {std::clamp<float>(fx, 0.0f, 1.0f), std::clamp<float>(fy, 0.0f, 1.0f)};
}

void EmuWindow_SDL2::OnMouseButton(u32 button, u8 state, s32 x, s32 y) {
    const auto mouse_button = SDLButtonToMouseButton(button);
    if (state == SDL_PRESSED) {
        const auto [touch_x, touch_y] = MouseToTouchPos(x, y);
        input_subsystem->GetMouse()->PressButton(x, y, mouse_button);
        input_subsystem->GetMouse()->PressMouseButton(mouse_button);
        input_subsystem->GetMouse()->PressTouchButton(touch_x, touch_y, mouse_button);
    } else {
        input_subsystem->GetMouse()->ReleaseButton(mouse_button);
    }
}

void EmuWindow_SDL2::OnMouseMotion(s32 x, s32 y) {
    const auto [touch_x, touch_y] = MouseToTouchPos(x, y);
    input_subsystem->GetMouse()->Move(x, y, 0, 0);
    input_subsystem->GetMouse()->MouseMove(touch_x, touch_y);
    input_subsystem->GetMouse()->TouchMove(touch_x, touch_y);
}

void EmuWindow_SDL2::OnFingerDown(float x, float y, std::size_t id) {
    input_subsystem->GetTouchScreen()->TouchPressed(x, y, id);
}

void EmuWindow_SDL2::OnFingerMotion(float x, float y, std::size_t id) {
    input_subsystem->GetTouchScreen()->TouchMoved(x, y, id);
}

void EmuWindow_SDL2::OnFingerUp() {
    input_subsystem->GetTouchScreen()->ReleaseAllTouch();
}

void EmuWindow_SDL2::OnKeyEvent(int key, u8 state) {
#ifdef _WIN32
    if (state == SDL_PRESSED && key == SDL_SCANCODE_F12) {
        ShowDevMenu(system, input_subsystem);
        return;
    }
#endif
    if (state == SDL_PRESSED) {
        input_subsystem->GetKeyboard()->PressKey(static_cast<std::size_t>(key));
    } else if (state == SDL_RELEASED) {
        input_subsystem->GetKeyboard()->ReleaseKey(static_cast<std::size_t>(key));
    }
}

bool EmuWindow_SDL2::IsOpen() const {
    return is_open;
}

bool EmuWindow_SDL2::IsShown() const {
    return is_shown;
}

void EmuWindow_SDL2::OnResize() {
    int width, height;
    SDL_GetWindowSizeInPixels(render_window, &width, &height);
    UpdateCurrentFramebufferLayout(width, height);
}

void EmuWindow_SDL2::ShowCursor(bool show_cursor) {
    if (show_cursor) {
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }
}

void EmuWindow_SDL2::Fullscreen() {
    switch (Settings::values.fullscreen_mode.GetValue()) {
    case Settings::FullscreenMode::Exclusive:
        // Set window size to render size before entering fullscreen -- SDL3 does not resize window
        // to display dimensions automatically in this mode.
        {
            const SDL_DisplayMode* display_mode =
                SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
            if (display_mode) {
                SDL_SetWindowSize(render_window, display_mode->w, display_mode->h);
            } else {
                LOG_ERROR(Frontend, "SDL_GetDesktopDisplayMode failed: {}", SDL_GetError());
            }
        }

        if (SDL_SetWindowFullscreen(render_window, true)) {
            return;
        }

        LOG_ERROR(Frontend, "Fullscreening failed: {}", SDL_GetError());
        LOG_INFO(Frontend, "Attempting to use borderless fullscreen...");
        [[fallthrough]];
    case Settings::FullscreenMode::Borderless:
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

void EmuWindow_SDL2::WaitEvent() {
    // Called on main thread
    SDL_Event event;

    if (!SDL_WaitEvent(&event)) {
        const char* error = SDL_GetError();
        if (!error || strcmp(error, "") == 0) {
            // https://github.com/libsdl-org/SDL/issues/5780
            // Sometimes SDL will return without actually having hit an error condition;
            // just ignore it in this case.
            return;
        }

        LOG_CRITICAL(Frontend, "SDL_WaitEvent failed: {}", error);
        exit(1);
    }

    switch (event.type) {
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
        OnResize();
        break;
    case SDL_EVENT_WINDOW_MINIMIZED:
        is_shown = false;
        OnResize();
        break;
    case SDL_EVENT_WINDOW_EXPOSED:
        is_shown = true;
        OnResize();
        break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        is_open = false;
        break;
    case SDL_EVENT_KEY_DOWN:
        OnKeyEvent(static_cast<int>(event.key.scancode), SDL_PRESSED);
        break;
    case SDL_EVENT_KEY_UP:
        OnKeyEvent(static_cast<int>(event.key.scancode), SDL_RELEASED);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        // ignore if it came from touch
        if (event.motion.which != SDL_TOUCH_MOUSEID)
            OnMouseMotion(static_cast<s32>(event.motion.x), static_cast<s32>(event.motion.y));
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        // ignore if it came from touch
        if (event.button.which != SDL_TOUCH_MOUSEID) {
            OnMouseButton(event.button.button,
                          event.button.down ? SDL_PRESSED : SDL_RELEASED,
                          static_cast<s32>(event.button.x), static_cast<s32>(event.button.y));
        }
        break;
    case SDL_EVENT_FINGER_DOWN:
        OnFingerDown(event.tfinger.x, event.tfinger.y,
                     static_cast<std::size_t>(event.tfinger.fingerID));
        break;
    case SDL_EVENT_FINGER_MOTION:
        OnFingerMotion(event.tfinger.x, event.tfinger.y,
                       static_cast<std::size_t>(event.tfinger.fingerID));
        break;
    case SDL_EVENT_FINGER_UP:
        OnFingerUp();
        break;
    case SDL_EVENT_QUIT:
        is_open = false;
        break;
    default:
        break;
    }

    const u64 current_time = SDL_GetTicks();
    if (current_time > last_time + 2000) {
        const auto results = system.GetAndResetPerfStats();
        std::string game_name;
        [[maybe_unused]] auto _ = system.GetGameName(game_name);
        if (g_native_export_mode) {
            // Standalone game export: plain game title, no emulator branding.
            if (!game_name.empty()) {
                SDL_SetWindowTitle(render_window, game_name.c_str());
            }
        } else {
            const auto title =
                fmt::format("{} | {} | FPS: {:.0f} ({:.0f}%)", game_name.empty() ? "suyu" : game_name,
                            Common::g_build_fullname, results.average_game_fps,
                            results.emulation_speed * 100.0);
            SDL_SetWindowTitle(render_window, title.c_str());
        }
        last_time = current_time;
    }
}

// Credits to Samantas5855 and others for this function.
void EmuWindow_SDL2::SetWindowIcon() {
#ifdef _WIN32
    // Native game exports embed the ROM's own icon into the exe's PE resources
    // (RT_GROUP_ICON id 1, see suyu/game_export.cpp) — use that instead of the
    // suyu logo so the window reads as the game, not the emulator.
    if (g_native_export_mode) {
        const HICON hicon = static_cast<HICON>(
            LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, 256, 256,
                       LR_DEFAULTCOLOR));
        if (hicon != nullptr) {
            ICONINFO info{};
            if (GetIconInfo(hicon, &info)) {
                BITMAP bmp{};
                GetObjectW(info.hbmColor, sizeof(bmp), &bmp);
                const int w = bmp.bmWidth;
                const int h = bmp.bmHeight;
                std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * h * 4);
                BITMAPINFO bi{};
                bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bi.bmiHeader.biWidth = w;
                bi.bmiHeader.biHeight = -h; // top-down
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 32;
                bi.bmiHeader.biCompression = BI_RGB;
                const HDC hdc = GetDC(nullptr);
                if (GetDIBits(hdc, info.hbmColor, 0, h, pixels.data(), &bi, DIB_RGB_COLORS)) {
                    // BGRA -> RGBA
                    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
                        std::swap(pixels[i], pixels[i + 2]);
                    }
                    SDL_Surface* const icon_surface = SDL_CreateSurfaceFrom(
                        w, h, SDL_PIXELFORMAT_RGBA32, pixels.data(), w * 4);
                    if (icon_surface != nullptr) {
                        SDL_SetWindowIcon(render_window, icon_surface);
                        SDL_DestroySurface(icon_surface);
                        ReleaseDC(nullptr, hdc);
                        DeleteObject(info.hbmColor);
                        DeleteObject(info.hbmMask);
                        DestroyIcon(hicon);
                        return;
                    }
                }
                ReleaseDC(nullptr, hdc);
                DeleteObject(info.hbmColor);
                DeleteObject(info.hbmMask);
            }
            DestroyIcon(hicon);
        }
        LOG_WARNING(Frontend, "Native export: failed to load game icon from exe resources, "
                               "falling back to suyu icon.");
    }
#endif
    SDL_IOStream* const suyu_icon_stream = SDL_IOFromConstMem((void*)suyu_icon, suyu_icon_size);
    if (suyu_icon_stream == nullptr) {
        LOG_WARNING(Frontend, "Failed to create suyu icon stream.");
        return;
    }
    SDL_Surface* const window_icon = SDL_LoadBMP_IO(suyu_icon_stream, true);
    if (window_icon == nullptr) {
        LOG_WARNING(Frontend, "Failed to read BMP from stream.");
        return;
    }
    // The icon is attached to the window pointer
    SDL_SetWindowIcon(render_window, window_icon);
    SDL_DestroySurface(window_icon);
}

void EmuWindow_SDL2::OnMinimalClientAreaChangeRequest(std::pair<u32, u32> minimal_size) {
    SDL_SetWindowMinimumSize(render_window, minimal_size.first, minimal_size.second);
}
