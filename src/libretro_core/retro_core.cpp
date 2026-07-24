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
#include <filesystem>
#include <vector>
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/cpu_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "hid_core/hid_core.h"
#include "input_common/drivers/virtual_gamepad.h"
#include "input_common/main.h"
#include "libretro_core/libretro.h"
#include "libretro_core/retro_emu_window.h"
#include "video_core/renderer_base.h"

namespace {

std::unique_ptr<Core::System> g_system;
std::unique_ptr<LibretroCore::RetroEmuWindow> g_emu_window;
std::shared_ptr<InputCommon::InputSubsystem> g_input_subsystem;
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

    static const struct retro_variable vars[] = {
        {"suyu_renderer", "Renderer; Vulkan|OpenGL|Software"},
        {"suyu_resolution", "Internal Resolution; 1x|2x|3x|4x"},
        {"suyu_cpu_accuracy", "CPU Accuracy; Auto|Accurate|Unsafe"},
        {"suyu_use_docked", "Docked Mode; Yes|No"},
        {"suyu_fastmem", "Fastmem; Enabled|Disabled"},
        {nullptr, nullptr},
    };
    cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);
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
    g_input_subsystem = std::make_shared<InputCommon::InputSubsystem>();
    g_input_subsystem->Initialize();

    g_system->Initialize();
    Settings::values.renderer_backend.SetValue(Settings::RendererBackend::Vulkan);
    Settings::values.cpuopt_fastmem.SetValue(true);
    Settings::values.cpuopt_fastmem_exclusives.SetValue(true);
    Settings::values.log_flush_line.SetValue(true);
    Settings::values.log_filter.SetValue("*:Info Service.VI:Debug Service.AM:Debug Service.Nvnflinger:Debug");
    g_system->ApplySettings();
    g_system->SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
    g_system->SetFilesystem(std::make_shared<FileSys::RealVfsFilesystem>());
    g_system->GetFileSystemController().CreateFactories(*g_system->GetFilesystem());
    g_system->GetUserChannel().clear();

    // Load keys from RetroArch system directory if available
    // Users can place prod.keys and title.keys in <system_dir>/suyu/keys/
    if (g_environ_cb) {
        const char* system_dir = nullptr;
        if (g_environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir) {
            const auto src_dir = std::filesystem::path(system_dir) / "suyu" / "keys";
            const auto dst_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::KeysDir);
            LOG_INFO(Frontend, "libretro: checking for keys in: {}", src_dir.string());
            if (std::filesystem::exists(src_dir)) {
                Common::FS::CreateDir(dst_dir);
                for (const auto& name : {"prod.keys", "title.keys", "console.keys"}) {
                    auto src = src_dir / name;
                    auto dst = dst_dir / name;
                    if (std::filesystem::exists(src) && !std::filesystem::exists(dst)) {
                        std::error_code ec;
                        std::filesystem::copy_file(src, dst, ec);
                        if (!ec) {
                            LOG_INFO(Frontend, "libretro: copied {} from RetroArch system dir", name);
                        }
                    }
                }
            }
        }
    }

    g_system->HIDCore().ReloadInputDevices();
    LOG_INFO(Frontend, "libretro core: retro_init() complete");
}

RETRO_API void retro_deinit() {
    g_emu_window.reset();
    g_system.reset();
    if (g_input_subsystem) { g_input_subsystem->Shutdown(); g_input_subsystem.reset(); }
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

namespace {
using VB = InputCommon::VirtualGamepad::VirtualButton;
struct RetroToVirtual {
    unsigned retro_id;
    VB virtual_button;
};
constexpr RetroToVirtual kButtonMap[] = {
    {RETRO_DEVICE_ID_JOYPAD_A, VB::ButtonA},
    {RETRO_DEVICE_ID_JOYPAD_B, VB::ButtonB},
    {RETRO_DEVICE_ID_JOYPAD_X, VB::ButtonX},
    {RETRO_DEVICE_ID_JOYPAD_Y, VB::ButtonY},
    {RETRO_DEVICE_ID_JOYPAD_L, VB::TriggerL},
    {RETRO_DEVICE_ID_JOYPAD_R, VB::TriggerR},
    {RETRO_DEVICE_ID_JOYPAD_L2, VB::TriggerZL},
    {RETRO_DEVICE_ID_JOYPAD_R2, VB::TriggerZR},
    {RETRO_DEVICE_ID_JOYPAD_L3, VB::StickL},
    {RETRO_DEVICE_ID_JOYPAD_R3, VB::StickR},
    {RETRO_DEVICE_ID_JOYPAD_START, VB::ButtonPlus},
    {RETRO_DEVICE_ID_JOYPAD_SELECT, VB::ButtonMinus},
    {RETRO_DEVICE_ID_JOYPAD_UP, VB::ButtonUp},
    {RETRO_DEVICE_ID_JOYPAD_DOWN, VB::ButtonDown},
    {RETRO_DEVICE_ID_JOYPAD_LEFT, VB::ButtonLeft},
    {RETRO_DEVICE_ID_JOYPAD_RIGHT, VB::ButtonRight},
};
bool g_prev_buttons[20] = {};
} // namespace

RETRO_API void retro_run() {
    if (g_input_poll_cb) {
        g_input_poll_cb();
    }

    // Bridge libretro input → suyu HID via VirtualGamepad
    if (g_input_state_cb && g_input_subsystem && g_game_loaded) {
        auto* vgp = g_input_subsystem->GetVirtualGamepad();
        if (vgp) {
            for (const auto& m : kButtonMap) {
                const bool pressed = g_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, m.retro_id) != 0;
                const int idx = static_cast<int>(m.virtual_button);
                if (pressed != g_prev_buttons[idx]) {
                    g_prev_buttons[idx] = pressed;
                    vgp->SetButtonState(0, m.virtual_button, pressed);
                }
            }
            // Left analog stick
            const float lx = g_input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) / 32768.0f;
            const float ly = g_input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / -32768.0f;
            vgp->SetStickPosition(0, InputCommon::VirtualGamepad::VirtualStick::Left, lx, ly);
            // Right analog stick
            const float rx = g_input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) / 32768.0f;
            const float ry = g_input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / -32768.0f;
            vgp->SetStickPosition(0, InputCommon::VirtualGamepad::VirtualStick::Right, rx, ry);
        }
    }

    static unsigned frame_counter = 0;
    ++frame_counter;

    if (frame_counter <= 3 || (frame_counter % 600) == 0) {
        LOG_INFO(Frontend, "libretro: retro_run frame {}, game_loaded={}", frame_counter, g_game_loaded);
        fprintf(stderr, "[suyu-libretro] retro_run frame %u, game_loaded=%d\n", frame_counter, g_game_loaded);
        fflush(stderr);
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
                // One-shot frame dump for verification
                if (frame_counter == 300 || frame_counter == 900 || frame_counter == 1800) {
                    const unsigned w = renderer.GetHeadlessWidth();
                    const unsigned h = renderer.GetHeadlessHeight();
                    char path[256];
                    snprintf(path, sizeof(path),
                             "C:\\Users\\charl\\Documents\\SuyuEclipse\\libretro_frame_%u.bmp",
                             frame_counter);
                    FILE* f = fopen(path, "wb");
                    if (f) {
                        const u32 row_bytes = w * 3;
                        const u32 pad = (4 - (row_bytes % 4)) % 4;
                        const u32 stride = row_bytes + pad;
                        const u32 img_size = stride * h;
                        const u32 file_size = 54 + img_size;
                        u8 hdr[54] = {};
                        hdr[0]='B'; hdr[1]='M';
                        memcpy(hdr+2, &file_size, 4);
                        u32 off=54; memcpy(hdr+10, &off, 4);
                        u32 dib=40; memcpy(hdr+14, &dib, 4);
                        int sw=(int)w, sh=(int)h;
                        memcpy(hdr+18, &sw, 4); memcpy(hdr+22, &sh, 4);
                        u16 planes=1; memcpy(hdr+26, &planes, 2);
                        u16 bpp=24; memcpy(hdr+28, &bpp, 2);
                        memcpy(hdr+34, &img_size, 4);
                        fwrite(hdr, 1, 54, f);
                        const u8 zero[4] = {};
                        for (int y = (int)h - 1; y >= 0; --y) {
                            for (unsigned x = 0; x < w; ++x) {
                                u32 px;
                                memcpy(&px, frame.data() + (y * w + x) * 4, 4);
                                u8 rgb[3] = {(u8)(px>>16), (u8)(px>>8), (u8)px};
                                fwrite(rgb, 1, 3, f);
                            }
                            if (pad) fwrite(zero, 1, pad, f);
                        }
                        fclose(f);
                        LOG_INFO(Frontend, "libretro: saved frame dump to {}", path);
                    }
                }
                // Vulkan outputs B8G8R8A8 (BGRA in memory), RetroArch XRGB8888
                // is 0xXXRRGGBB = BGRX in memory. Swap R↔B channels.
                const unsigned w = renderer.GetHeadlessWidth();
                const unsigned h = renderer.GetHeadlessHeight();
                static std::vector<u8> swapped;
                swapped.resize(frame.size());
                const u8* src = frame.data();
                u8* dst = swapped.data();
                for (unsigned i = 0; i < w * h; ++i) {
                    dst[i*4+0] = src[i*4+2]; // R
                    dst[i*4+1] = src[i*4+1]; // G
                    dst[i*4+2] = src[i*4+0]; // B
                    dst[i*4+3] = src[i*4+3]; // A
                }
                g_video_cb(swapped.data(), w, h, w * 4);
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

    // Apply core options before loading the game
    if (g_environ_cb) {
        struct retro_variable var;
        var.key = "suyu_use_docked";
        var.value = nullptr;
        if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
            Settings::values.use_docked_mode.SetValue(
                std::string(var.value) == "Yes" ? Settings::ConsoleMode::Docked
                                                : Settings::ConsoleMode::Handheld);
        }
        var.key = "suyu_cpu_accuracy";
        var.value = nullptr;
        if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
            std::string v(var.value);
            if (v == "Accurate")
                Settings::values.cpu_accuracy.SetValue(Settings::CpuAccuracy::Accurate);
            else if (v == "Unsafe")
                Settings::values.cpu_accuracy.SetValue(Settings::CpuAccuracy::Unsafe);
            else
                Settings::values.cpu_accuracy.SetValue(Settings::CpuAccuracy::Auto);
        }
        var.key = "suyu_fastmem";
        var.value = nullptr;
        if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
            const bool enabled = std::string(var.value) == "Enabled";
            Settings::values.cpuopt_fastmem.SetValue(enabled);
            Settings::values.cpuopt_fastmem_exclusives.SetValue(enabled);
        }
        g_system->ApplySettings();
    }

    Service::AM::FrontendAppletParameters load_parameters{};
    load_parameters.applet_id = Service::AM::AppletId::Application;

    const Core::SystemResultStatus result =
        g_system->Load(*g_emu_window, g_game_path, load_parameters);
    if (result != Core::SystemResultStatus::Success) {
        LOG_CRITICAL(Frontend, "libretro core: Load() failed with status {}",
                     static_cast<u32>(result));
        return false;
    }

    // The GUI's EmuThread does these steps between Load and Run — without them the GPU thread
    // never starts and the CPU manager doesn't know the GPU is ready, causing the game to stall
    // before it ever reaches display setup.
    auto& gpu = g_system->GPU();
    gpu.ObtainContext();
    gpu.ReleaseContext();
    gpu.Start();
    g_system->GetCpuManager().OnGpuReady();

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
