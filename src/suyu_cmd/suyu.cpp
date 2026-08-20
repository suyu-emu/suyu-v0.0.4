// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <thread>

#include <fmt/ostream.h>

#include "common/detached_tasks.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/fs/path_util.h"
#include "common/nvidia_flags.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/arm/recomp/arm_recomp.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/cpu_manager.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/am/service/library_applet_creator.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/loader.h"
#include "frontend_common/config.h"
#include "input_common/main.h"
#include "network/network.h"
#include "sdl_config.h"
#include "suyu_cmd/emu_window/emu_window_sdl2.h"
#include "suyu_cmd/emu_window/emu_window_sdl2_gl.h"
#ifdef __APPLE__
#include "suyu_cmd/emu_window/emu_window_sdl2_mtl.h"
#endif
#include "suyu_cmd/emu_window/emu_window_sdl2_null.h"
#include "suyu_cmd/emu_window/emu_window_sdl2_vk.h"
#include "video_core/renderer_base.h"

#ifdef _WIN32
// windows.h needs to be included before shellapi.h
#include <windows.h>

#include <shellapi.h>

#include "common/windows/timer_resolution.h"
#endif

#undef _UNICODE
#include <getopt.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifdef _WIN32
extern "C" {
// tells Nvidia and AMD drivers to use the dedicated GPU by default on laptops with switchable
// graphics
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#ifdef __unix__
#include "common/linux/gamemode.h"
#endif

// Statically linked recompiled CPU modules.
//
// A per-game build of this executable (see SUYU_CMD_RECOMP_DIR in
// src/suyu_cmd/CMakeLists.txt) compiles in a generated recomp_registration.c
// that lists the game's modules in NSO load order. That makes the exported
// game a single self-contained binary - there is nothing to LoadLibrary and no
// recompiled_*.dll to ship beside it. The layout below must match the struct
// the generator emits.
extern "C" {
struct SuyuRecompStaticModule {
    const char* name;
    void (*(*lookup)(u64))(void*);
    void (*set_base)(u64);
};
#ifdef SUYU_CMD_STATIC_RECOMP
const SuyuRecompStaticModule* suyu_recomp_static_modules(unsigned* count);
#endif
}

static void PrintHelp(const char* argv0) {
    std::cout << "Usage: " << argv0
              << " [options]\n"
                 "-c, --config          Load the specified configuration file\n"
                 "-f, --fullscreen      Start in fullscreen mode\n"
                 "-g, --game            File path of the game to load\n"
                 "-h, --help            Display this help and exit\n"
                 "-m, --multiplayer=nick:password@address:port"
                 " Nickname, password, address and port for multiplayer\n"
                 "-p, --program         Pass following string as arguments to executable\n"
                 "-u, --user            Select a specific user profile from 0 to 7\n"
                 "-v, --version         Output version information and exit\n"
                 "-l, "
                 "--applet-params="
                 "\"program_id,applet_id,applet_type,launch_type,prog_index,prev_prog_index\"\n"
                 "                      Numerical parameters for launching an applet. If no\n"
                 "                      game is provided, then the applet will launch off of\n"
                 "                      the applet_id.\n";
}

static void PrintVersion() {
    std::cout << "suyu" << Common::g_scm_branch << " " << Common::g_scm_desc << std::endl;
}

static void OnStateChanged(const Network::RoomMember::State& state) {
    switch (state) {
    case Network::RoomMember::State::Idle:
        LOG_DEBUG(Network, "Network is idle");
        break;
    case Network::RoomMember::State::Joining:
        LOG_DEBUG(Network, "Connection sequence to room started");
        break;
    case Network::RoomMember::State::Joined:
        LOG_DEBUG(Network, "Successfully joined to the room");
        break;
    case Network::RoomMember::State::Moderator:
        LOG_DEBUG(Network, "Successfully joined the room as a moderator");
        break;
    default:
        break;
    }
}

static void OnNetworkError(const Network::RoomMember::Error& error) {
    switch (error) {
    case Network::RoomMember::Error::LostConnection:
        LOG_DEBUG(Network, "Lost connection to the room");
        break;
    case Network::RoomMember::Error::CouldNotConnect:
        LOG_ERROR(Network, "Error: Could not connect");
        exit(1);
        break;
    case Network::RoomMember::Error::NameCollision:
        LOG_ERROR(
            Network,
            "You tried to use the same nickname as another user that is connected to the Room");
        exit(1);
        break;
    case Network::RoomMember::Error::IpCollision:
        LOG_ERROR(Network, "You tried to use the same fake IP-Address as another user that is "
                           "connected to the Room");
        exit(1);
        break;
    case Network::RoomMember::Error::WrongPassword:
        LOG_ERROR(Network, "Room replied with: Wrong password");
        exit(1);
        break;
    case Network::RoomMember::Error::WrongVersion:
        LOG_ERROR(Network,
                  "You are using a different version than the room you are trying to connect to");
        exit(1);
        break;
    case Network::RoomMember::Error::RoomIsFull:
        LOG_ERROR(Network, "The room is full");
        exit(1);
        break;
    case Network::RoomMember::Error::HostKicked:
        LOG_ERROR(Network, "You have been kicked by the host");
        break;
    case Network::RoomMember::Error::HostBanned:
        LOG_ERROR(Network, "You have been banned by the host");
        break;
    case Network::RoomMember::Error::UnknownError:
        LOG_ERROR(Network, "UnknownError");
        break;
    case Network::RoomMember::Error::PermissionDenied:
        LOG_ERROR(Network, "PermissionDenied");
        break;
    case Network::RoomMember::Error::NoSuchUser:
        LOG_ERROR(Network, "NoSuchUser");
        break;
    }
}

static void OnMessageReceived(const Network::ChatEntry& msg) {
    std::cout << std::endl << msg.nickname << ": " << msg.message << std::endl << std::endl;
}

static void OnStatusMessageReceived(const Network::StatusMessageEntry& msg) {
    std::string message;
    switch (msg.type) {
    case Network::IdMemberJoin:
        message = fmt::format("{} has joined", msg.nickname);
        break;
    case Network::IdMemberLeave:
        message = fmt::format("{} has left", msg.nickname);
        break;
    case Network::IdMemberKicked:
        message = fmt::format("{} has been kicked", msg.nickname);
        break;
    case Network::IdMemberBanned:
        message = fmt::format("{} has been banned", msg.nickname);
        break;
    case Network::IdAddressUnbanned:
        message = fmt::format("{} has been unbanned", msg.nickname);
        break;
    }
    if (!message.empty())
        std::cout << std::endl << "* " << message << std::endl << std::endl;
}

/// True once native recompiled CPU modules are registered — the running
/// process is a standalone game export, not the suyu dev frontend.
bool g_native_export_mode = false;

/// Application entry point
int main(int argc, char** argv) {
#ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "wb", stdout);
        freopen("CONOUT$", "wb", stderr);
    }
#endif

    try {

    // ── Portable user data for standalone game exports ──────────────────────
    // An exported game is a self-contained folder the user can move or delete
    // as a unit, so its config/saves/NAND/logs/screenshots live in <exe>/user
    // rather than %APPDATA%\suyu. KeysDir must be explicitly pinned at
    // %APPDATA%\suyu\keys (not left to derive on its own): the presence of a
    // sibling "user" folder next to the exe makes the FS layer's own
    // portable-mode auto-detection kick in first and silently rederive
    // KeysDir under <exe>/user/keys instead, so prod.keys/title.keys are
    // never bundled with a distributed export.
    // Must run before Log::Initialize(), which opens a file under LogDir.
#ifdef SUYU_CMD_STATIC_RECOMP
    {
        namespace FS = Common::FS;
#ifdef _WIN32
        wchar_t exe_w[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exe_w, MAX_PATH);
        const std::filesystem::path user_root =
            std::filesystem::path(exe_w).parent_path() / L"user";
#else
        const std::filesystem::path user_root =
            std::filesystem::path(argv[0]).parent_path() / "user";
#endif
        std::filesystem::create_directories(user_root);
        // SetEdenPath (path_util.cpp) fails with "is not a directory" if the
        // path doesn't exist yet - most of these get created lazily by
        // whatever subsystem first writes into them, but LoadDir/TASDir are
        // read from (mod scan, TAS script lookup) before anything writes to
        // them, so create every subdir up front instead of relying on that.
        for (const char* sub : {"config", "cache", "cache/shader", "log", "nand", "sdmc", "dump",
                                 "load", "screenshots", "play_time", "crash_dumps", "amiibo", "tas",
                                 "icons", "themes"}) {
            std::filesystem::create_directories(user_root / sub);
        }
        FS::SetEdenPath(FS::EdenPath::EdenDir, user_root);
        FS::SetEdenPath(FS::EdenPath::ConfigDir, user_root / "config");
        FS::SetEdenPath(FS::EdenPath::CacheDir, user_root / "cache");
        FS::SetEdenPath(FS::EdenPath::ShaderDir, user_root / "cache" / "shader");
        FS::SetEdenPath(FS::EdenPath::LogDir, user_root / "log");
        FS::SetEdenPath(FS::EdenPath::NANDDir, user_root / "nand");
        FS::SetEdenPath(FS::EdenPath::SaveDir, user_root / "nand");
        FS::SetEdenPath(FS::EdenPath::SDMCDir, user_root / "sdmc");
        FS::SetEdenPath(FS::EdenPath::DumpDir, user_root / "dump");
        FS::SetEdenPath(FS::EdenPath::LoadDir, user_root / "load");
        FS::SetEdenPath(FS::EdenPath::ScreenshotsDir, user_root / "screenshots");
        FS::SetEdenPath(FS::EdenPath::PlayTimeDir, user_root / "play_time");
        FS::SetEdenPath(FS::EdenPath::CrashDumpsDir, user_root / "crash_dumps");
        FS::SetEdenPath(FS::EdenPath::AmiiboDir, user_root / "amiibo");
        FS::SetEdenPath(FS::EdenPath::TASDir, user_root / "tas");
        FS::SetEdenPath(FS::EdenPath::IconsDir, user_root / "icons");
        FS::SetEdenPath(FS::EdenPath::ThemesDir, user_root / "themes");
        FS::SetEdenPath(FS::EdenPath::KeysDir, FS::GetAppDataRoamingDirectory() / "suyu" / "keys");
    }
#endif

    Common::Log::Initialize();
    Common::Log::SetColorConsoleBackendEnabled(true);
    Common::Log::Start();
    LOG_INFO(Frontend, "suyu-cmd starting up...");
    Common::DetachedTasks detached_tasks;

    int option_index = 0;
#ifdef _WIN32
    int argc_w;
    auto argv_w = CommandLineToArgvW(GetCommandLineW(), &argc_w);

    if (argv_w == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to get command line arguments");
        return -1;
    }
#endif
    std::string filepath;
    std::optional<std::string> config_path;
    std::string program_args;
    std::optional<int> selected_user;

    bool use_multiplayer = false;
    bool fullscreen = false;
    Service::AM::FrontendAppletParameters load_parameters{};
    std::string nickname{};
    std::string password{};
    std::string address{};
    u16 port = Network::DefaultRoomPort;

    static struct option long_options[] = {
        // clang-format off
        {"config", required_argument, 0, 'c'},
        {"fullscreen", no_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {"game", required_argument, 0, 'g'},
        {"applet-params", optional_argument, 0, 'l'},
        {"multiplayer", required_argument, 0, 'm'},
        {"program", optional_argument, 0, 'p'},
        {"user", required_argument, 0, 'u'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0},
        // clang-format on
    };

    while (optind < argc) {
        int arg = getopt_long(argc, argv, "g:fhvp::c:u:l::", long_options, &option_index);
        if (arg != -1) {
            switch (static_cast<char>(arg)) {
            case 'c':
                config_path = optarg;
                break;
            case 'f':
                fullscreen = true;
                LOG_INFO(Frontend, "Starting in fullscreen mode...");
                break;
            case 'h':
                PrintHelp(argv[0]);
                return 0;
            case 'g': {
                const std::string str_arg(optarg);
                filepath = str_arg;
                break;
            }
            case 'l': {
                std::string str_arg(argv[optind++]);
                str_arg.append(",0"); // FALLBACK: if string is partially completed ("1234,3")
                                      // this will set all those unset to 0. otherwise we get
                                      // all 3s.
                std::stringstream stream(str_arg);
                std::string sub;
                std::getline(stream, sub, ',');
                load_parameters.program_id = std::stoull(sub);
                std::getline(stream, sub, ',');
                load_parameters.applet_id = static_cast<Service::AM::AppletId>(std::stoul(sub));
                std::getline(stream, sub, ',');
                load_parameters.applet_type = static_cast<Service::AM::AppletType>(std::stoi(sub));
                std::getline(stream, sub, ',');
                load_parameters.launch_type = static_cast<Service::AM::LaunchType>(std::stoi(sub));
                std::getline(stream, sub, ',');
                load_parameters.program_index = std::stoi(sub);
                std::getline(stream, sub, ',');
                load_parameters.previous_program_index = std::stoi(sub);
                break;
            }
            case 'm': {
                use_multiplayer = true;
                const std::string str_arg(optarg);
                // regex to check if the format is nickname:password@ip:port
                // with optional :password
                const std::regex re("^([^:]+)(?::(.+))?@([^:]+)(?::([0-9]+))?$");
                if (!std::regex_match(str_arg, re)) {
                    std::cout << "Wrong format for option --multiplayer\n";
                    PrintHelp(argv[0]);
                    return 0;
                }

                std::smatch match;
                std::regex_search(str_arg, match, re);
                ASSERT(match.size() == 5);
                nickname = match[1];
                password = match[2];
                address = match[3];
                if (!match[4].str().empty()) {
                    port = static_cast<u16>(std::strtoul(match[4].str().c_str(), nullptr, 0));
                }
                std::regex nickname_re("^[a-zA-Z0-9._\\- ]+$");
                if (!std::regex_match(nickname, nickname_re)) {
                    std::cout
                        << "Nickname is not valid. Must be 4 to 20 alphanumeric characters.\n";
                    return 0;
                }
                if (address.empty()) {
                    std::cout << "Address to room must not be empty.\n";
                    return 0;
                }
                break;
            }
            case 'p':
                program_args = argv[optind];
                ++optind;
                break;
            case 'u':
                selected_user = atoi(optarg);
                break;
            case 'v':
                PrintVersion();
                return 0;
            }
        } else {
#ifdef _WIN32
            filepath = Common::UTF16ToUTF8(argv_w[optind]);
#else
            filepath = argv[optind];
#endif
            optind++;
        }
    }

    SdlConfig config{config_path};

    // apply the log_filter setting
    // the logger was initialized before and doesn't pick up the filter on its own
    Common::Log::Filter filter;
    filter.ParseFilterString(Settings::values.log_filter.GetValue());
    Common::Log::SetGlobalFilter(filter);

    if (!program_args.empty()) {
        Settings::values.program_args = program_args;
    }

    if (selected_user.has_value()) {
        Settings::values.current_user = std::clamp(*selected_user, 0, 7);
    }

#ifdef _WIN32
    LocalFree(argv_w);
#endif

    MicroProfileOnThreadCreate("EmuThread");
    SCOPE_EXIT {
        MicroProfileShutdown();
    };

    Common::ConfigureNvidiaEnvironmentFlags();

    // Auto-detect ROM / exefs alongside the executable when no -g flag is given
    if (filepath.empty() && !static_cast<u32>(load_parameters.applet_id)) {
#ifdef _WIN32
        wchar_t exe_path_w[MAX_PATH];
        GetModuleFileNameW(nullptr, exe_path_w, MAX_PATH);
        const std::filesystem::path exe_dir = std::filesystem::path(exe_path_w).parent_path();
#else
        const std::filesystem::path exe_dir =
            std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
        // Prefer deconstructed exefs dir (Switch ROM viewer structure: exefs/main)
        const std::filesystem::path exefs_main = exe_dir / "exefs" / "main";
        if (std::filesystem::exists(exefs_main)) {
#ifdef _WIN32
            filepath = Common::UTF16ToUTF8(exefs_main.wstring());
#else
            filepath = exefs_main.string();
#endif
            LOG_INFO(Frontend, "Auto-detected exefs/main: {}", filepath);
            goto rom_found;
        }
        // Fall back to packed ROM files (XCI/NSP/NCA)
        static constexpr std::array<std::string_view, 3> exts{".xci", ".nsp", ".nca"};
        for (const auto& entry : std::filesystem::directory_iterator(exe_dir)) {
            const auto ext = Common::ToLower(entry.path().extension().string());
            for (const auto e : exts) {
                if (ext == e) {
#ifdef _WIN32
                    filepath = Common::UTF16ToUTF8(entry.path().wstring());
#else
                    filepath = entry.path().string();
#endif
                    LOG_INFO(Frontend, "Auto-detected ROM: {}", filepath);
                    goto rom_found;
                }
            }
        }
        LOG_CRITICAL(Frontend, "No ROM specified, no exefs/main found, and no XCI/NSP/NCA next to exe");
        return -1;
        rom_found:;
    }

    // Native recompiled CPU modules, in NSO load order: rtld(0), main(1),
    // subsdk0-N(2..N+1), sdk(last). Whichever way they arrive, registering any
    // of them makes ArmRecomp run the game's CPU natively instead of dynarmic.
    struct RecompModule {
        Core::RecompBlockFn (*lookup)(u64){};
        void (*set_base)(u64){};
    };
    static std::vector<RecompModule> s_recomp_modules;

    // Preferred path: modules compiled straight into this executable. Nothing
    // to find on disk, nothing to load, and no version skew between the exe and
    // its modules.
#ifdef SUYU_CMD_STATIC_RECOMP
    {
        unsigned count = 0;
        const SuyuRecompStaticModule* mods = suyu_recomp_static_modules(&count);
        for (unsigned i = 0; i < count; ++i) {
            s_recomp_modules.push_back({mods[i].lookup, mods[i].set_base});
            LOG_INFO(Frontend, "Static recompiled module [{}] {} — ArmRecomp active", i,
                     mods[i].name ? mods[i].name : "?");
        }
    }
#endif

    // Compatibility path for exports that ship recompiled_*.dll beside the exe:
    // recompiled_rtld.dll, recompiled_image.dll (main), recompiled_subsdk0.dll,
    // recompiled_sdk.dll. Skipped entirely when modules are already linked in.
#ifdef _WIN32
    if (s_recomp_modules.empty()) {
        wchar_t _exe_w[MAX_PATH]{};
        GetModuleFileNameW(nullptr, _exe_w, MAX_PATH);
        const auto _exe_dir = std::filesystem::path(_exe_w).parent_path();

        // Load in standard NSO load order: rtld, main (recompiled_image), subsdk0..9, sdk
        std::vector<std::wstring> dll_order = {
            L"recompiled_rtld.dll",
            L"recompiled_image.dll",  // main
            L"recompiled_subsdk0.dll", L"recompiled_subsdk1.dll", L"recompiled_subsdk2.dll",
            L"recompiled_subsdk3.dll", L"recompiled_subsdk4.dll", L"recompiled_subsdk5.dll",
            L"recompiled_subsdk6.dll", L"recompiled_subsdk7.dll", L"recompiled_subsdk8.dll",
            L"recompiled_subsdk9.dll",
            L"recompiled_sdk.dll",
        };

        for (const auto& dll_name : dll_order) {
            const auto p = _exe_dir / dll_name;
            if (!std::filesystem::exists(p)) continue;
            HMODULE h = LoadLibraryW(p.wstring().c_str());
            if (!h) {
                LOG_WARNING(Frontend, "Found {} but LoadLibrary failed (err={})",
                            Common::UTF16ToUTF8(dll_name), GetLastError());
                continue;
            }
            using LookupFn = Core::RecompBlockFn (*)(u64);
            using SetBaseFn = void (*)(u64);
            auto lkp = reinterpret_cast<LookupFn>(GetProcAddress(h, "recomp_image_lookup"));
            auto sbf = reinterpret_cast<SetBaseFn>(GetProcAddress(h, "recomp_image_set_base"));
            if (lkp) {
                s_recomp_modules.push_back({lkp, sbf});
                LOG_INFO(Frontend, "Native recompiled module [{}] loaded from {} — ArmRecomp active",
                         s_recomp_modules.size() - 1, Common::UTF16ToUTF8(dll_name));
            }
        }
    }
#endif

    if (!s_recomp_modules.empty()) {
        // Combined lookup: try each module's lookup until one returns non-null.
        Core::SetRecompLookup([](u64 pc) -> Core::RecompBlockFn {
            for (const auto& m : s_recomp_modules) {
                if (auto fn = m.lookup(pc)) return fn;
            }
            return nullptr;
        });
        // Route base to the module at the same index in load order.
        // rtld=index0, main=index1, subsdk0=index2, ..., sdk=last.
        Core::SetRecompBaseSetter([](size_t index, const char*, u64 base) {
            if (index < s_recomp_modules.size() && s_recomp_modules[index].set_base) {
                s_recomp_modules[index].set_base(base);
            }
        });
        // A window running native recompiled code is a standalone game export,
        // not the suyu dev frontend — the window chrome (title/icon) should
        // read as the game, not the emulator.
        g_native_export_mode = true;

        // Decode video on the CPU unless the user has chosen otherwise.
        // Handing the guest's VP9 streams to a hardware decoder (d3d11va on
        // Windows) deadlocks partway through the first movie on at least Intel
        // integrated graphics: the process stays alive and the log stops
        // mid-line, which presents as a permanently black window right after
        // boot. An export is something a player double-clicks with no
        // settings UI in front of them, so it defaults to the path that always
        // finishes over the one that is faster when it works.
        if (Settings::values.nvdec_emulation.UsingGlobal() &&
            Settings::values.nvdec_emulation.GetValue() == Settings::NvdecEmulation::Gpu) {
            Settings::values.nvdec_emulation.SetValue(Settings::NvdecEmulation::Cpu);
            LOG_INFO(Frontend, "Native export: using CPU video decoding");
        }
    }

    // Mods/patches: a standalone export is a self-contained folder, so a "mods"
    // directory beside the executable is where users will drop things. Point
    // the existing load directory at it and the normal PatchManager path
    // (LayeredFS, IPS/pchtxt patches, cheats) picks it up unchanged - same
    // <title_id>/<mod name>/ layout the Qt frontend uses.
    {
#ifdef _WIN32
        wchar_t exe_w[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exe_w, MAX_PATH);
        const auto exe_dir = std::filesystem::path(exe_w).parent_path();
#else
        const auto exe_dir = std::filesystem::path(argv[0]).parent_path();
#endif
        const auto local_mods = exe_dir / "mods";
        std::error_code ec;
        std::filesystem::create_directories(local_mods, ec);
        if (std::filesystem::is_directory(local_mods)) {
            Common::FS::SetEdenPath(Common::FS::EdenPath::LoadDir, local_mods);
            LOG_INFO(Frontend, "Using local mod directory: {}", local_mods.string());
        }
        LOG_INFO(Frontend, "Keys directory (never bundled): {}",
                 Common::FS::GetEdenPathString(Common::FS::EdenPath::KeysDir));
    }

    LOG_INFO(Frontend, "suyu-cmd: Initializing system...");
    Core::System system{};
    system.Initialize();
    LOG_INFO(Frontend, "suyu-cmd: System initialized.");

    InputCommon::InputSubsystem input_subsystem{};

    // Apply the command line arguments
    system.ApplySettings();

    std::unique_ptr<EmuWindow_SDL2> emu_window;
    switch (Settings::values.renderer_backend.GetValue()) {
    case Settings::RendererBackend::OpenGL_GLSL:
    case Settings::RendererBackend::OpenGL_GLASM:
    case Settings::RendererBackend::OpenGL_SPIRV:
        emu_window = std::make_unique<EmuWindow_SDL2_GL>(&input_subsystem, system, fullscreen);
        break;
    case Settings::RendererBackend::Vulkan:
        emu_window = std::make_unique<EmuWindow_SDL2_VK>(&input_subsystem, system, fullscreen);
        break;
#ifdef __APPLE__
    case Settings::RendererBackend::Metal:
        emu_window = std::make_unique<EmuWindow_SDL2_MTL>(&input_subsystem, system, fullscreen);
        break;
#endif
    case Settings::RendererBackend::Null:
        emu_window = std::make_unique<EmuWindow_SDL2_Null>(&input_subsystem, system, fullscreen);
        break;
    default:
        emu_window = std::make_unique<EmuWindow_SDL2_VK>(&input_subsystem, system, fullscreen);
        break;
    }

#ifdef _WIN32
    Common::Windows::SetCurrentTimerResolutionToMaximum();
    system.CoreTiming().SetTimerResolutionNs(Common::Windows::GetCurrentTimerResolution());
#endif

    LOG_INFO(Frontend, "suyu-cmd: Window created, loading game...");
    system.SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
    system.SetFilesystem(std::make_shared<FileSys::RealVfsFilesystem>());
    system.GetFileSystemController().CreateFactories(*system.GetFilesystem());
    system.GetUserChannel().clear();

    if (static_cast<u32>(load_parameters.applet_id)) {
        // code below based off of suyu/main.cpp : GMainWindow::OnHomeMenu()
        // Inline minimal mapping (AppletIdToProgramId is in an anonymous namespace)
        const auto applet_id_to_prog_id = [](Service::AM::AppletId id) -> Service::AM::AppletProgramId {
            using namespace Service::AM;
            switch (id) {
            case AppletId::QLaunch:        return AppletProgramId::QLaunch;
            case AppletId::Starter:        return AppletProgramId::Starter;
            case AppletId::Auth:           return AppletProgramId::Auth;
            case AppletId::OverlayDisplay: return AppletProgramId::OverlayDisplay;
            default:                       return static_cast<AppletProgramId>(0);
            }
        };
        Service::AM::AppletProgramId applet_prog_id = applet_id_to_prog_id(load_parameters.applet_id);
        auto sysnand = system.GetFileSystemController().GetSystemNANDContents();
        if (!sysnand) {
            LOG_CRITICAL(Frontend, "Failed to load applet: Firmware not installed.");
            return -1;
        }

        auto user_applet_nca = sysnand->GetEntry(static_cast<u64>(applet_prog_id),
                                                 FileSys::ContentRecordType::Program);
        if (!user_applet_nca) {
            LOG_CRITICAL(Frontend, "Failed to load applet: applet cannot be found.");
            return -1;
        }
        if (filepath.empty())
            filepath = user_applet_nca->GetFullPath();
    } else {
        load_parameters.applet_id = Service::AM::AppletId::Application;
    }
    LOG_INFO(Frontend, "suyu-cmd: Calling system.Load for '{}'...", filepath);
    const Core::SystemResultStatus load_result{system.Load(*emu_window, filepath, load_parameters)};
    LOG_INFO(Frontend, "suyu-cmd: system.Load returned: {}", static_cast<int>(load_result));

    switch (load_result) {
    case Core::SystemResultStatus::ErrorGetLoader:
        LOG_CRITICAL(Frontend, "Failed to obtain loader for {}!", filepath);
        return -1;
    case Core::SystemResultStatus::ErrorLoader:
        LOG_CRITICAL(Frontend, "Failed to load ROM!");
        return -1;
    case Core::SystemResultStatus::ErrorNotInitialized:
        LOG_CRITICAL(Frontend, "CPUCore not initialized");
        return -1;
    case Core::SystemResultStatus::ErrorVideoCore:
        LOG_CRITICAL(Frontend, "Failed to initialize VideoCore!");
        return -1;
    case Core::SystemResultStatus::Success:
        break; // Expected case
    default:
        if (static_cast<u32>(load_result) >
            static_cast<u32>(Core::SystemResultStatus::ErrorLoader)) {
            const u16 loader_id = static_cast<u16>(Core::SystemResultStatus::ErrorLoader);
            const u16 error_id = static_cast<u16>(load_result) - loader_id;
            LOG_CRITICAL(Frontend,
                         "While attempting to load the ROM requested, an error occurred. Please "
                         "refer to the suyu wiki for more information or the suyu discord for "
                         "additional help.\n\nError Code: {:04X}-{:04X}\nError Description: {}",
                         loader_id, error_id, static_cast<Loader::ResultStatus>(error_id));
        }
        break;
    }

    if (use_multiplayer) {
        // This Eden merge dropped Core::System::GetRoomNetwork() - the direct-
        // connect room networking suyu-cmd's --multiplayer flag relies on no
        // longer exists upstream. Fail the flag loudly instead of building
        // against a member that isn't there; re-wiring direct-connect against
        // whatever replaced it is a separate task, not something to guess at.
        LOG_ERROR(Network,
                  "suyu-cmd: --multiplayer is unavailable in this build (room networking was "
                  "removed upstream)");
        return 0;
    }

    // Core is loaded, start the GPU (makes the GPU contexts current to this thread)
    system.GPU().Start();
    system.GetCpuManager().OnGpuReady();

    // A game export is launched over and over by a player, always for the same
    // title, so the disk shader cache is the difference between a long black
    // screen on every single run and one slow first run. Keep it for exports
    // and keep the old blanket disable for the plain dev frontend, where the
    // startup instability it works around was originally seen.
    if (!g_native_export_mode && Settings::values.use_disk_shader_cache.GetValue()) {
        LOG_WARNING(Frontend,
                    "suyu-cmd: disabling disk shader cache for this run to avoid known startup instability");
        Settings::values.use_disk_shader_cache.SetValue(false);
    }

    if (Settings::values.use_disk_shader_cache.GetValue()) {
        try {
            system.Renderer().ReadRasterizer()->LoadDiskResources(
                system.GetApplicationProcessProgramID(), std::stop_token{},
                [](VideoCore::LoadCallbackStage, size_t value, size_t total) {});
        } catch (const std::exception& e) {
            LOG_ERROR(Frontend, "Failed to load disk shader cache: {}", e.what());
        } catch (...) {
            LOG_ERROR(Frontend, "Failed to load disk shader cache due to unknown exception");
        }
    }

    system.RegisterExitCallback([&] {
        // Just exit right away.
        exit(0);
    });

#ifdef __unix__
    Common::Linux::StartGamemode();
#endif

    void(system.Run());
    if (system.DebuggerEnabled()) {
        system.InitializeDebugger();
    }
    while (emu_window->IsOpen()) {
        emu_window->WaitEvent();
    }
    system.DetachDebugger();
    void(system.Pause());
    system.ShutdownMainProcess();

#ifdef __unix__
    Common::Linux::StopGamemode();
#endif

    detached_tasks.WaitForAllTasks();
    return 0;
    } catch (const std::exception& e) {
        LOG_CRITICAL(Frontend, "Unhandled fatal exception in suyu-cmd: {}", e.what());
        return -1;
    } catch (...) {
        LOG_CRITICAL(Frontend, "Unhandled unknown fatal exception in suyu-cmd");
        return -1;
    }
}
