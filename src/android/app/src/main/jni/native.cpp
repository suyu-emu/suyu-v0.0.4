// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#define VMA_IMPLEMENTATION
#include "video_core/vulkan_common/vma.h"

#include <codecvt>
#include <cstdio>
#include <cstring>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <dlfcn.h>
#include <unistd.h>

#include <iostream>
#ifdef ARCHITECTURE_arm64
#include <adrenotools/driver.h>
#endif

#include <android/api-level.h>
#include <android/native_window_jni.h>
#include <common/fs/fs.h>
#include <core/file_sys/patch_manager.h>
#include <core/file_sys/savedata_factory.h>
#include <core/loader/nro.h>
#include <frontend_common/content_manager.h>
#include <jni.h>

#include "common/android/multiplayer/multiplayer.h"
#include "common/android/android_common.h"
#include "common/android/id_cache.h"
#include "common/detached_tasks.h"
#include "common/dynamic_library.h"
#include "common/fs/path_util.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "frontend_common/play_time_manager.h"
#include "core/constants.h"
#include "core/core.h"
#include "core/cpu_manager.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/fs_filesystem.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/submission_package.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/frontend/applets/cabinet.h"
#include "core/frontend/applets/controller.h"
#include "core/frontend/applets/error.h"
#include "core/frontend/applets/general.h"
#include "core/frontend/applets/mii_edit.h"
#include "core/frontend/applets/profile_select.h"
#include "core/frontend/applets/software_keyboard.h"
#include "core/frontend/applets/web_browser.h"
#include "common/android/applets/web_browser.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/am/frontend/applets.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/set/system_settings_server.h"
#include "core/loader/loader.h"
#include "frontend_common/config.h"
#include "frontend_common/firmware_manager.h"
#ifdef ENABLE_UPDATE_CHECKER
#include "frontend_common/update_checker.h"
#endif
#include "hid_core/frontend/emulated_controller.h"
#include "hid_core/hid_core.h"
#include "hid_core/hid_types.h"
#include "input_common/drivers/virtual_amiibo.h"
#include "jni/native.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/capture.h"
#include "video_core/textures/decoders.h"
#include "video_core/vulkan_common/vulkan_instance.h"
#include "video_core/vulkan_common/vulkan_surface.h"
#include "video_core/shader_notify.h"
#include "network/announce_multiplayer_session.h"

#define jconst [[maybe_unused]] const auto
#define jauto [[maybe_unused]] auto

static EmulationSession s_instance;

//Abdroid Multiplayer which can be initialized with parameters
std::unique_ptr<AndroidMultiplayer> multiplayer{nullptr};
std::shared_ptr<Core::AnnounceMultiplayerSession> announce_multiplayer_session;

//Power Status default values
std::atomic<int> g_battery_percentage = {100};
std::atomic<bool> g_is_charging = {false};
std::atomic<bool> g_has_battery = {true};

// playtime
std::unique_ptr<PlayTime::PlayTimeManager> play_time_manager;

EmulationSession::EmulationSession() {
    m_vfs = std::make_shared<FileSys::RealVfsFilesystem>();
}

EmulationSession& EmulationSession::GetInstance() {
    return s_instance;
}

const Core::System& EmulationSession::System() const {
    return m_system;
}

Core::System& EmulationSession::System() {
    return m_system;
}

FileSys::ManualContentProvider* EmulationSession::GetContentProvider() {
    return m_manual_provider.get();
}

InputCommon::InputSubsystem& EmulationSession::GetInputSubsystem() {
    return m_input_subsystem;
}

const EmuWindow_Android& EmulationSession::Window() const {
    return *m_window;
}

EmuWindow_Android& EmulationSession::Window() {
    return *m_window;
}

ANativeWindow* EmulationSession::NativeWindow() const {
    return m_native_window;
}

void EmulationSession::SetNativeWindow(ANativeWindow* native_window) {
    m_native_window = native_window;
}

void EmulationSession::InitializeGpuDriver(const std::string& hook_lib_dir,
                                           const std::string& custom_driver_dir,
                                           const std::string& custom_driver_name,
                                           const std::string& file_redirect_dir) {
#ifdef ARCHITECTURE_arm64
    void* handle{};
    const char* file_redirect_dir_{};
    int featureFlags{};

    // Enable driver file redirection when renderer debugging is enabled.
    if (Settings::values.renderer_debug && file_redirect_dir.size()) {
        featureFlags |= ADRENOTOOLS_DRIVER_FILE_REDIRECT;
        file_redirect_dir_ = file_redirect_dir.c_str();
    }

    // Try to load a custom driver.
    if (custom_driver_name.size()) {
        handle = adrenotools_open_libvulkan(
            RTLD_NOW, featureFlags | ADRENOTOOLS_DRIVER_CUSTOM, nullptr, hook_lib_dir.c_str(),
            custom_driver_dir.c_str(), custom_driver_name.c_str(), file_redirect_dir_, nullptr);
    }

    // Try to load the system driver.
    if (!handle) {
        handle = adrenotools_open_libvulkan(RTLD_NOW, featureFlags, nullptr, hook_lib_dir.c_str(),
                                            nullptr, nullptr, file_redirect_dir_, nullptr);
    }

    m_vulkan_library = std::make_shared<Common::DynamicLibrary>(handle);
#endif
}

bool EmulationSession::IsRunning() const {
    return m_is_running;
}

bool EmulationSession::IsPaused() const {
    return m_is_running && m_is_paused;
}

const Core::PerfStatsResults& EmulationSession::PerfStats() {
    m_perf_stats = m_system.GetAndResetPerfStats();
    return m_perf_stats;
}

int EmulationSession::ShadersBuilding() {
    auto& shader_notify = m_system.GPU().ShaderNotify();
    m_shaders_building = shader_notify.ShadersBuilding();
    return m_shaders_building;
}

void EmulationSession::SurfaceChanged() {
    if (!IsRunning()) {
        return;
    }
    m_window->OnSurfaceChanged(m_native_window);
}

void EmulationSession::ConfigureFilesystemProvider(const std::string& filepath) {
    const auto file = m_system.GetFilesystem()->OpenFile(filepath, FileSys::OpenMode::Read);
    if (!file) {
        return;
    }

    const auto extension = Common::ToLower(filepath.substr(filepath.find_last_of('.') + 1));

    if (extension == "nsp") {
        auto nsp = std::make_shared<FileSys::NSP>(file);
        if (nsp->GetStatus() == Loader::ResultStatus::Success) {
            std::map<u64, u32> nsp_versions;
            std::map<u64, std::string> nsp_version_strings;

            for (const auto& [title_id, nca_map] : nsp->GetNCAs()) {
                for (const auto& [type_pair, nca] : nca_map) {
                    const auto& [title_type, content_type] = type_pair;

                    if (content_type == FileSys::ContentRecordType::Meta) {
                        const auto meta_nca = std::make_shared<FileSys::NCA>(nca->GetBaseFile());
                        if (meta_nca->GetStatus() == Loader::ResultStatus::Success) {
                            const auto section0 = meta_nca->GetSubdirectories();
                            if (!section0.empty()) {
                                for (const auto& meta_file : section0[0]->GetFiles()) {
                                    if (meta_file->GetExtension() == "cnmt") {
                                        FileSys::CNMT cnmt(meta_file);
                                        nsp_versions[cnmt.GetTitleID()] = cnmt.GetTitleVersion();
                                    }
                                }
                            }
                        }
                    }

                    if (content_type == FileSys::ContentRecordType::Control &&
                        title_type == FileSys::TitleType::Update) {
                        auto romfs = nca->GetRomFS();
                        if (romfs) {
                            auto extracted = FileSys::ExtractRomFS(romfs);
                            if (extracted) {
                                auto nacp_file = extracted->GetFile("control.nacp");
                                if (!nacp_file) {
                                    nacp_file = extracted->GetFile("Control.nacp");
                                }
                                if (nacp_file) {
                                    FileSys::NACP nacp(nacp_file);
                                    auto ver_str = nacp.GetVersionString();
                                    if (!ver_str.empty()) {
                                        nsp_version_strings[title_id] = ver_str;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            for (const auto& [title_id, nca_map] : nsp->GetNCAs()) {
                for (const auto& [type_pair, nca] : nca_map) {
                    const auto& [title_type, content_type] = type_pair;

                    if (title_type == FileSys::TitleType::Update) {
                        u32 version = 0;
                        auto ver_it = nsp_versions.find(title_id);
                        if (ver_it != nsp_versions.end()) {
                            version = ver_it->second;
                        }

                        std::string version_string;
                        auto str_it = nsp_version_strings.find(title_id);
                        if (str_it != nsp_version_strings.end()) {
                            version_string = str_it->second;
                        }

                        m_manual_provider->AddEntryWithVersion(
                            title_type, content_type, title_id, version, version_string,
                            nca->GetBaseFile());

                        LOG_DEBUG(Frontend, "Added NSP update entry - TitleID: {:016X}, Version: {}, VersionStr: {}",
                                  title_id, version, version_string);
                    } else {
                        // Use regular AddEntry for non-updates
                        m_manual_provider->AddEntry(title_type, content_type, title_id,
                                                    nca->GetBaseFile());
                        LOG_DEBUG(Frontend, "Added NSP entry - TitleID: {:016X}, TitleType: {}, ContentType: {}",
                                  title_id, static_cast<int>(title_type), static_cast<int>(content_type));
                    }
                }
            }
            return;
        }
    }

    // Handle XCI files
    if (extension == "xci") {
        FileSys::XCI xci{file};
        if (xci.GetStatus() == Loader::ResultStatus::Success) {
            const auto nsp = xci.GetSecurePartitionNSP();
            if (nsp) {
                for (const auto& title : nsp->GetNCAs()) {
                    for (const auto& entry : title.second) {
                        m_manual_provider->AddEntry(entry.first.first, entry.first.second, title.first,
                                                    entry.second->GetBaseFile());
                    }
                }
            }
            return;
        }
    }

    auto loader = Loader::GetLoader(m_system, file);
    if (!loader) {
        return;
    }

    const auto file_type = loader->GetFileType();
    if (file_type == Loader::FileType::Unknown || file_type == Loader::FileType::Error) {
        return;
    }

    u64 program_id = 0;
    const auto res2 = loader->ReadProgramId(program_id);
    if (res2 == Loader::ResultStatus::Success && file_type == Loader::FileType::NCA) {
        m_manual_provider->AddEntry(FileSys::TitleType::Application,
                                    FileSys::GetCRTypeFromNCAType(FileSys::NCA{file}.GetType()),
                                    program_id, file);
    }
}

void EmulationSession::InitializeSystem(bool reload) {
    if (!reload) {
        // Initialize logging system
        Common::Log::Initialize();
        Common::Log::SetColorConsoleBackendEnabled(true);
        Common::Log::Start();

        m_input_subsystem.Initialize();
    }

    // Initialize filesystem.
    m_system.SetFilesystem(m_vfs);
    m_system.GetUserChannel().clear();
    m_manual_provider = std::make_unique<FileSys::ManualContentProvider>();
    m_system.SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
    m_system.RegisterContentProvider(FileSys::ContentProviderUnionSlot::FrontendManual,
                                     m_manual_provider.get());
    m_system.GetFileSystemController().CreateFactories(*m_vfs);
}

void EmulationSession::SetAppletId(int applet_id) {
    m_applet_id = applet_id;
    m_system.GetFrontendAppletHolder().SetCurrentAppletId(
        static_cast<Service::AM::AppletId>(m_applet_id));
}

Core::SystemResultStatus EmulationSession::InitializeEmulation(const std::string& filepath,
                                                               const std::size_t program_index,
                                                               const bool frontend_initiated) {
    std::scoped_lock lock(m_mutex);

    // Create the render window.
    m_window = std::make_unique<EmuWindow_Android>(m_native_window, m_vulkan_library);

    // Initialize system.
    jauto android_keyboard = std::make_unique<Common::Android::SoftwareKeyboard::AndroidKeyboard>();
    jauto android_webapplet = std::make_unique<Common::Android::WebBrowser::AndroidWebBrowser>();
    m_software_keyboard = android_keyboard.get();
    m_system.SetShuttingDown(false);
    m_system.ApplySettings();
    Settings::LogSettings();
    m_system.HIDCore().ReloadInputDevices();
    m_system.SetFrontendAppletSet({
        nullptr,                     // Amiibo Settings
        nullptr,                     // Controller Selector
        nullptr,                     // Error Display
        nullptr,                     // Mii Editor
        nullptr,                     // Parental Controls
        nullptr,                     // Photo Viewer
        nullptr,                     // Profile Selector
        std::move(android_keyboard), // Software Keyboard
        std::move(android_webapplet),// Web Browser
        nullptr,                     // Net Connect
    });

    // Initialize filesystem.
    ConfigureFilesystemProvider(filepath);

    // Load the ROM.
    Service::AM::FrontendAppletParameters params{
        .applet_id = static_cast<Service::AM::AppletId>(m_applet_id),
        .launch_type = frontend_initiated ? Service::AM::LaunchType::FrontendInitiated
                                          : Service::AM::LaunchType::ApplicationInitiated,
        .program_index = static_cast<s32>(program_index),
    };

    m_load_result = m_system.Load(EmulationSession::GetInstance().Window(), filepath, params);
    if (m_load_result != Core::SystemResultStatus::Success) {
        return m_load_result;
    }

    // Complete initialization.
    m_system.GPU().Start();
    m_system.GetCpuManager().OnGpuReady();
    m_system.RegisterExitCallback([&] { HaltEmulation(); });

    // Register an ExecuteProgram callback such that Core can execute a sub-program
    m_system.RegisterExecuteProgramCallback([&](std::size_t program_index_) {
        m_next_program_index = program_index_;
        EmulationSession::GetInstance().HaltEmulation();
    });

    OnEmulationStarted();
    return Core::SystemResultStatus::Success;
}

void EmulationSession::ShutdownEmulation() {
    std::scoped_lock lock(m_mutex);

    if (m_next_program_index != -1) {
        ChangeProgram(m_next_program_index);
        m_next_program_index = -1;
    }

    m_is_running = false;

    // Unload user input.
    m_system.HIDCore().UnloadInputDevices();

    // Enable all controllers
    m_system.HIDCore().SetSupportedStyleTag({Core::HID::NpadStyleSet::All});

    // Shutdown the main emulated process
    if (m_load_result == Core::SystemResultStatus::Success) {
        m_system.DetachDebugger();
        m_system.ShutdownMainProcess();
        m_detached_tasks.WaitForAllTasks();
        m_load_result = Core::SystemResultStatus::ErrorNotInitialized;
        m_window.reset();
        OnEmulationStopped(Core::SystemResultStatus::Success);
        return;
    }

    // Tear down the render window.
    m_window.reset();
}

void EmulationSession::PauseEmulation() {
    std::scoped_lock lock(m_mutex);
    m_system.Pause();
    m_is_paused = true;
}

void EmulationSession::UnPauseEmulation() {
    std::scoped_lock lock(m_mutex);
    m_system.Run();
    m_is_paused = false;
}

void EmulationSession::HaltEmulation() {
    std::scoped_lock lock(m_mutex);
    m_is_running = false;
    m_cv.notify_one();
}

void EmulationSession::RunEmulation() {
    {
        std::scoped_lock lock(m_mutex);
        m_is_running = true;
    }

    // Load the disk shader cache.
    if (Settings::values.use_disk_shader_cache.GetValue()) {
        LoadDiskCacheProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);
        m_system.Renderer().ReadRasterizer()->LoadDiskResources(
            m_system.GetApplicationProcessProgramID(), std::stop_token{}, LoadDiskCacheProgress);
        LoadDiskCacheProgress(VideoCore::LoadCallbackStage::Complete, 0, 0);
    }

    void(m_system.Run());

    if (m_system.DebuggerEnabled()) {
        m_system.InitializeDebugger();
    }

    while (true) {
        {
            [[maybe_unused]] std::unique_lock lock(m_mutex);
            if (m_cv.wait_for(lock, std::chrono::milliseconds(800),
                              [&]() { return !m_is_running; })) {
                // Emulation halted.
                break;
            }
        }
    }

    // Reset current applet ID.
    m_applet_id = static_cast<int>(Service::AM::AppletId::Application);
}

Common::Android::SoftwareKeyboard::AndroidKeyboard* EmulationSession::SoftwareKeyboard() {
    return m_software_keyboard;
}

void EmulationSession::LoadDiskCacheProgress(VideoCore::LoadCallbackStage stage, int progress,
                                             int max) {
    JNIEnv* env = Common::Android::GetEnvForThread();
    env->CallStaticVoidMethod(Common::Android::GetDiskCacheProgressClass(),
                              Common::Android::GetDiskCacheLoadProgress(), static_cast<jint>(stage),
                              static_cast<jint>(progress), static_cast<jint>(max));
}

void EmulationSession::OnEmulationStarted() {
    JNIEnv* env = Common::Android::GetEnvForThread();
    env->CallStaticVoidMethod(Common::Android::GetNativeLibraryClass(),
                              Common::Android::GetOnEmulationStarted());
}

void EmulationSession::OnEmulationStopped(Core::SystemResultStatus result) {
    JNIEnv* env = Common::Android::GetEnvForThread();
    env->CallStaticVoidMethod(Common::Android::GetNativeLibraryClass(),
                              Common::Android::GetOnEmulationStopped(), static_cast<jint>(result));
}

void EmulationSession::ChangeProgram(std::size_t program_index) {
    JNIEnv* env = Common::Android::GetEnvForThread();
    env->CallStaticVoidMethod(Common::Android::GetNativeLibraryClass(),
                              Common::Android::GetOnProgramChanged(),
                              static_cast<jint>(program_index));
}

u64 EmulationSession::GetProgramId(JNIEnv* env, jstring jprogramId) {
    auto program_id_string = Common::Android::GetJString(env, jprogramId);
    try {
        return std::stoull(program_id_string);
    } catch (...) {
        return 0;
    }
}

static Core::SystemResultStatus RunEmulation(const std::string& filepath,
                                             const size_t program_index,
                                             const bool frontend_initiated) {
    LOG_INFO(Frontend, "starting");

    if (filepath.empty()) {
        LOG_CRITICAL(Frontend, "failed to load: filepath empty!");
        return Core::SystemResultStatus::ErrorLoader;
    }

    SCOPE_EXIT {
        EmulationSession::GetInstance().ShutdownEmulation();
    };

    jconst result = EmulationSession::GetInstance().InitializeEmulation(filepath, program_index,
                                                                        frontend_initiated);
    if (result != Core::SystemResultStatus::Success) {
        return result;
    }

    EmulationSession::GetInstance().RunEmulation();

    return Core::SystemResultStatus::Success;
}

namespace {

struct CpuPartInfo {
    u32 vendor;
    u32 part;
    const char* name;
};

constexpr CpuPartInfo s_cpu_list[] = {
    // ARM - 0x41
    {0x41, 0xd01, "Cortex-A32"},
    {0x41, 0xd02, "Cortex-A34"},
    {0x41, 0xd04, "Cortex-A35"},
    {0x41, 0xd03, "Cortex-A53"},
    {0x41, 0xd05, "Cortex-A55"},
    {0x41, 0xd46, "Cortex-A510"},
    {0x41, 0xd80, "Cortex-A520"},
    {0x41, 0xd88, "Cortex-A520AE"},
    {0x41, 0xd07, "Cortex-A57"},
    {0x41, 0xd06, "Cortex-A65"},
    {0x41, 0xd43, "Cortex-A65AE"},
    {0x41, 0xd08, "Cortex-A72"},
    {0x41, 0xd09, "Cortex-A73"},
    {0x41, 0xd0a, "Cortex-A75"},
    {0x41, 0xd0b, "Cortex-A76"},
    {0x41, 0xd0e, "Cortex-A76AE"},
    {0x41, 0xd0d, "Cortex-A77"},
    {0x41, 0xd41, "Cortex-A78"},
    {0x41, 0xd42, "Cortex-A78AE"},
    {0x41, 0xd4b, "Cortex-A78C"},
    {0x41, 0xd47, "Cortex-A710"},
    {0x41, 0xd4d, "Cortex-A715"},
    {0x41, 0xd81, "Cortex-A720"},
    {0x41, 0xd89, "Cortex-A720AE"},
    {0x41, 0xd87, "Cortex-A725"},
    {0x41, 0xd44, "Cortex-X1"},
    {0x41, 0xd4c, "Cortex-X1C"},
    {0x41, 0xd48, "Cortex-X2"},
    {0x41, 0xd4e, "Cortex-X3"},
    {0x41, 0xd82, "Cortex-X4"},
    {0x41, 0xd85, "Cortex-X925"},
    {0x41, 0xd4a, "Neoverse E1"},
    {0x41, 0xd0c, "Neoverse N1"},
    {0x41, 0xd49, "Neoverse N2"},
    {0x41, 0xd8e, "Neoverse N3"},
    {0x41, 0xd40, "Neoverse V1"},
    {0x41, 0xd4f, "Neoverse V2"},
    {0x41, 0xd84, "Neoverse V3"},
    {0x41, 0xd83, "Neoverse V3AE"},
    // Qualcomm - 0x51
    {0x51, 0x201, "Kryo"},
    {0x51, 0x205, "Kryo"},
    {0x51, 0x211, "Kryo"},
    {0x51, 0x800, "Kryo 385 Gold"},
    {0x51, 0x801, "Kryo 385 Silver"},
    {0x51, 0x802, "Kryo 485 Gold"},
    {0x51, 0x803, "Kryo 485 Silver"},
    {0x51, 0x804, "Kryo 680 Prime"},
    {0x51, 0x805, "Kryo 680 Gold"},
    {0x51, 0x06f, "Krait"},
    {0x51, 0xc00, "Falkor"},
    {0x51, 0xc01, "Saphira"},
    {0x51, 0x001, "Oryon"},
};

const char* find_cpu_name(u32 vendor, u32 part) {
    for (const auto& cpu : s_cpu_list) {
        if (cpu.vendor == vendor && cpu.part == part) {
            return cpu.name;
        }
    }
    return nullptr;
}

u64 read_midr_sysfs(u32 cpu_id) {
    char path[128];
    std::snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%u/regs/identification/midr_el1", cpu_id);

    FILE* f = std::fopen(path, "r");
    if (!f) return 0;

    char value[32];
    if (!std::fgets(value, sizeof(value), f)) {
        std::fclose(f);
        return 0;
    }
    std::fclose(f);

    return std::strtoull(value, nullptr, 16);
}

std::pair<u32, std::string> get_pretty_cpus() {
    std::map<u64, int> core_layout;
    u32 valid_cpus = 0;
    for (u32 i = 0; i < std::thread::hardware_concurrency(); ++i) {
        const auto midr = read_midr_sysfs(i);
        if (midr == 0) break;

        valid_cpus++;
        core_layout[midr]++;
    }

    std::string cpus;

    if (!core_layout.empty()) {
        const CpuPartInfo* lowest_part = nullptr;
        u32 lowest_part_id = 0xFFFFFFFF;

        for (const auto& [midr, count] : core_layout) {
            const auto vendor = (midr >> 24) & 0xff;
            const auto part = (midr >> 4) & 0xfff;

            if (!cpus.empty()) cpus += " + ";
            cpus += fmt::format("{}x {}", count, find_cpu_name(vendor, part));
        }
    }

    return {valid_cpus, cpus};
}

std::string get_arm_cpu_name() {
    std::map<u64, int> core_layout;
    for (u32 i = 0; i < std::thread::hardware_concurrency(); ++i) {
        const auto midr = read_midr_sysfs(i);
        if (midr == 0) break;

        core_layout[midr]++;
    }

    if (!core_layout.empty()) {
        const CpuPartInfo* lowest_part = nullptr;
        u32 lowest_part_id = 0xFFFFFFFF;

        for (const auto& [midr, count] : core_layout) {
            const auto vendor = (midr >> 24) & 0xff;
            const auto part = (midr >> 4) & 0xfff;

            for (const auto& cpu : s_cpu_list) {
                if (cpu.vendor == vendor && cpu.part == part) {
                    if (cpu.part < lowest_part_id) {
                        lowest_part_id = cpu.part;
                        lowest_part = &cpu;
                    }
                    break;
                }
            }
        }

        if (lowest_part) {
            return lowest_part->name;
        }
    }

    FILE* f = std::fopen("/proc/cpuinfo", "r");
    if (!f) return "";

    char buf[512];
    std::string result;

    auto trim = [](std::string& s) {
        const auto start = s.find_first_not_of(" \t\r\n");
        const auto end = s.find_last_not_of(" \t\r\n");
        s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    };

    while (std::fgets(buf, sizeof(buf), f)) {
        std::string line(buf);
        if (line.find("Hardware") == 0) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                result = line.substr(pos + 1);
                trim(result);
                break;
            }
        }
    }
    std::fclose(f);

    if (!result.empty()) {
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    }

    return result;
}

const char* fallback_cpu_detection() {
    static std::string s_result = []() -> std::string {
        std::string result = get_arm_cpu_name();
        if (result.empty()) {
            return "Cortex-A34";
        }
        return result;
    }();

    return s_result.c_str();
}

} // namespace

extern "C" {

void Java_org_yuzu_yuzu_1emu_NativeLibrary_surfaceChanged(JNIEnv* env, jobject instance,
                                                          [[maybe_unused]] jobject surf) {
    EmulationSession::GetInstance().SetNativeWindow(ANativeWindow_fromSurface(env, surf));
    EmulationSession::GetInstance().SurfaceChanged();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_surfaceDestroyed(JNIEnv* env, jobject instance) {
    if (auto* native_window = EmulationSession::GetInstance().NativeWindow(); native_window) {
        ANativeWindow_release(native_window);
    }
    EmulationSession::GetInstance().SetNativeWindow(nullptr);
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_setAppDirectory(JNIEnv* env, jobject instance,
                                                           [[maybe_unused]] jstring j_directory) {
    Common::FS::SetAppDirectory(Common::Android::GetJString(env, j_directory));
}

int Java_org_yuzu_yuzu_1emu_NativeLibrary_installFileToNand(JNIEnv* env, jobject instance,
                                                            jstring j_file, jobject jcallback) {
    auto jlambdaClass = env->GetObjectClass(jcallback);
    auto jlambdaInvokeMethod = env->GetMethodID(
        jlambdaClass, "invoke", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    const auto callback = [env, jcallback, jlambdaInvokeMethod](size_t max, size_t progress) {
        auto jwasCancelled = env->CallObjectMethod(jcallback, jlambdaInvokeMethod,
                                                   Common::Android::ToJDouble(env, max),
                                                   Common::Android::ToJDouble(env, progress));
        return Common::Android::GetJBoolean(env, jwasCancelled);
    };

    return static_cast<int>(
        ContentManager::InstallNSP(EmulationSession::GetInstance().System(),
                                   *EmulationSession::GetInstance().System().GetFilesystem(),
                                   Common::Android::GetJString(env, j_file), callback));
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_doesUpdateMatchProgram(JNIEnv* env, jobject jobj,
                                                                      jstring jprogramId,
                                                                      jstring jupdatePath) {
    u64 program_id = EmulationSession::GetProgramId(env, jprogramId);
    std::string updatePath = Common::Android::GetJString(env, jupdatePath);
    std::shared_ptr<FileSys::NSP> nsp = std::make_shared<FileSys::NSP>(
        EmulationSession::GetInstance().System().GetFilesystem()->OpenFile(
            updatePath, FileSys::OpenMode::Read));
    for (const auto& item : nsp->GetNCAs()) {
        for (const auto& nca_details : item.second) {
            if (nca_details.second->GetName().ends_with(".cnmt.nca")) {
                auto update_id = nca_details.second->GetTitleId() & ~0xFFFULL;
                if (update_id == program_id) {
                    return true;
                }
            }
        }
    }
    return false;
}

void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_initializeGpuDriver(JNIEnv* env,
                                                                       [[maybe_unused]] jclass clazz,
                                                                       jstring hook_lib_dir,
                                                                       jstring custom_driver_dir,
                                                                       jstring custom_driver_name,
                                                                       jstring file_redirect_dir) {
    // Log active Freedreno environment variables
    const char* tu_debug = getenv("TU_DEBUG");
    const char* fd_debug = getenv("FD_MESA_DEBUG");
    const char* ir3_debug = getenv("IR3_SHADER_DEBUG");
    const char* fd_rd_dump = getenv("FD_RD_DUMP");
    const char* tu_breadcrumbs = getenv("TU_BREADCRUMBS");

    if (tu_debug || fd_debug || ir3_debug || fd_rd_dump || tu_breadcrumbs) {
        LOG_INFO(Frontend, "[Freedreno] Initializing GPU driver with configuration:");
        if (tu_debug) LOG_INFO(Frontend, "[Freedreno]   TU_DEBUG={}", tu_debug);
        if (fd_debug) LOG_INFO(Frontend, "[Freedreno]   FD_MESA_DEBUG={}", fd_debug);
        if (ir3_debug) LOG_INFO(Frontend, "[Freedreno]   IR3_SHADER_DEBUG={}", ir3_debug);
        if (fd_rd_dump) LOG_INFO(Frontend, "[Freedreno]   FD_RD_DUMP={}", fd_rd_dump);
        if (tu_breadcrumbs) LOG_INFO(Frontend, "[Freedreno]   TU_BREADCRUMBS={}", tu_breadcrumbs);
    }

    EmulationSession::GetInstance().InitializeGpuDriver(
        Common::Android::GetJString(env, hook_lib_dir),
        Common::Android::GetJString(env, custom_driver_dir),
        Common::Android::GetJString(env, custom_driver_name),
        Common::Android::GetJString(env, file_redirect_dir));
}

[[maybe_unused]] static bool CheckKgslPresent() {
    constexpr auto KgslPath{"/dev/kgsl-3d0"};

    return access(KgslPath, F_OK) == 0;
}

[[maybe_unused]] bool SupportsCustomDriver() {
    return android_get_device_api_level() >= 28 && CheckKgslPresent();
}

jboolean JNICALL Java_org_yuzu_yuzu_1emu_utils_GpuDriverHelper_supportsCustomDriverLoading(
    JNIEnv* env, jobject instance) {
#ifdef ARCHITECTURE_arm64
    // If the KGSL device exists custom drivers can be loaded using adrenotools
    return SupportsCustomDriver();
#else
    return false;
#endif
}

jobjectArray Java_org_yuzu_yuzu_1emu_utils_GpuDriverHelper_getSystemDriverInfo(
    JNIEnv* env, jobject j_obj, jobject j_surf, jstring j_hook_lib_dir) {
#ifdef ARCHITECTURE_arm64
    const char* file_redirect_dir_{};
    int featureFlags{};
    std::string hook_lib_dir = Common::Android::GetJString(env, j_hook_lib_dir);
    auto handle = adrenotools_open_libvulkan(RTLD_NOW, featureFlags, nullptr, hook_lib_dir.c_str(),
                                             nullptr, nullptr, file_redirect_dir_, nullptr);
    auto driver_library = std::make_shared<Common::DynamicLibrary>(handle);
    InputCommon::InputSubsystem input_subsystem;
    auto window =
        std::make_unique<EmuWindow_Android>(ANativeWindow_fromSurface(env, j_surf), driver_library);

    Vulkan::vk::InstanceDispatch dld;
    Vulkan::vk::Instance vk_instance = Vulkan::CreateInstance(
        *driver_library, dld, VK_API_VERSION_1_1, Core::Frontend::WindowSystemType::Android);

    auto surface = Vulkan::CreateSurface(vk_instance, window->GetWindowInfo());

    auto device = Vulkan::CreateDevice(vk_instance, dld, *surface);

    auto driver_version = device.GetDriverVersion();
    auto version_string =
        fmt::format("{}.{}.{}", VK_API_VERSION_MAJOR(driver_version),
                    VK_API_VERSION_MINOR(driver_version), VK_API_VERSION_PATCH(driver_version));
    auto driver_name = device.GetDriverName();
#else
    auto driver_version = "1.0.0";
    auto version_string = "1.1.0"; //Assume lowest Vulkan level
    auto driver_name = "generic";
#endif
    jobjectArray j_driver_info = env->NewObjectArray(2, Common::Android::GetStringClass(), Common::Android::ToJString(env, version_string));
    env->SetObjectArrayElement(j_driver_info, 1, Common::Android::ToJString(env, driver_name));
    return j_driver_info;
}

jstring Java_org_yuzu_yuzu_1emu_utils_GpuDriverHelper_getGpuModel(JNIEnv *env, jobject j_obj, jobject j_surf, jstring j_hook_lib_dir) {
#ifdef ARCHITECTURE_arm64
    const char* file_redirect_dir_{};
    int featureFlags{};
    std::string hook_lib_dir = Common::Android::GetJString(env, j_hook_lib_dir);
    auto handle = adrenotools_open_libvulkan(RTLD_NOW, featureFlags, nullptr, hook_lib_dir.c_str(),
                                             nullptr, nullptr, file_redirect_dir_, nullptr);
    auto driver_library = std::make_shared<Common::DynamicLibrary>(handle);
    InputCommon::InputSubsystem input_subsystem;
    auto window =
            std::make_unique<EmuWindow_Android>(ANativeWindow_fromSurface(env, j_surf), driver_library);

    Vulkan::vk::InstanceDispatch dld;
    Vulkan::vk::Instance vk_instance = Vulkan::CreateInstance(
            *driver_library, dld, VK_API_VERSION_1_1, Core::Frontend::WindowSystemType::Android);

    auto surface = Vulkan::CreateSurface(vk_instance, window->GetWindowInfo());

    auto device = Vulkan::CreateDevice(vk_instance, dld, *surface);

    const std::string model_name{device.GetModelName()};

    window.release();

    return Common::Android::ToJString(env, model_name);
#else
    return Common::Android::ToJString(env, "no-info");
#endif
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_reloadKeys(JNIEnv* env, jclass clazz) {
    Core::Crypto::KeyManager::Instance().ReloadKeys();
    return static_cast<jboolean>(Core::Crypto::KeyManager::Instance().AreKeysLoaded());
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_unpauseEmulation(JNIEnv* env, jclass clazz) {
    EmulationSession::GetInstance().UnPauseEmulation();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_pauseEmulation(JNIEnv* env, jclass clazz) {
    EmulationSession::GetInstance().PauseEmulation();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_stopEmulation(JNIEnv* env, jclass clazz) {
    EmulationSession::GetInstance().HaltEmulation();
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_isRunning(JNIEnv* env, jclass clazz) {
    return static_cast<jboolean>(EmulationSession::GetInstance().IsRunning());
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_isPaused(JNIEnv* env, jclass clazz) {
    return static_cast<jboolean>(EmulationSession::GetInstance().IsPaused());
}

jbyteArray Java_org_yuzu_yuzu_1emu_NativeLibrary_getAppletCaptureBuffer(JNIEnv* env, jclass clazz) {
    using namespace VideoCore::Capture;

    if (!EmulationSession::GetInstance().IsRunning()) {
        return env->NewByteArray(0);
    }

    const auto tiled = EmulationSession::GetInstance().System().GPU().GetAppletCaptureBuffer();
    if (tiled.size() < TiledSize) {
        return env->NewByteArray(0);
    }

    std::vector<u8> linear(LinearWidth * LinearHeight * BytesPerPixel);
    Tegra::Texture::UnswizzleTexture(linear, tiled, BytesPerPixel, LinearWidth, LinearHeight,
                                     LinearDepth, BlockHeight, BlockDepth);

    auto buffer = env->NewByteArray(static_cast<jsize>(linear.size()));
    if (!buffer) {
        return env->NewByteArray(0);
    }

    env->SetByteArrayRegion(buffer, 0, static_cast<jsize>(linear.size()),
                            reinterpret_cast<const jbyte*>(linear.data()));
    return buffer;
}

jint Java_org_yuzu_yuzu_1emu_NativeLibrary_getAppletCaptureWidth(JNIEnv* env, jclass clazz) {
    return static_cast<jint>(VideoCore::Capture::LinearWidth);
}

jint Java_org_yuzu_yuzu_1emu_NativeLibrary_getAppletCaptureHeight(JNIEnv* env, jclass clazz) {
    return static_cast<jint>(VideoCore::Capture::LinearHeight);
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_initializeSystem(JNIEnv* env, jclass clazz,
                                                            jboolean reload) {
    // Initialize the emulated system.
    if (!reload) {
        EmulationSession::GetInstance().System().Initialize();
    }
    EmulationSession::GetInstance().InitializeSystem(reload);
}

jdoubleArray Java_org_yuzu_yuzu_1emu_NativeLibrary_getPerfStats(JNIEnv* env, jclass clazz) {
    jdoubleArray j_stats = env->NewDoubleArray(4);

    if (EmulationSession::GetInstance().IsRunning()) {
        jconst results = EmulationSession::GetInstance().PerfStats();

        // Converting the structure into an array makes it easier to pass it to the frontend
        double stats[4] = {results.system_fps, results.average_game_fps, results.frametime,
                           results.emulation_speed};

        env->SetDoubleArrayRegion(j_stats, 0, 4, stats);
    }

    return j_stats;
}

jint Java_org_yuzu_yuzu_1emu_NativeLibrary_getShadersBuilding(JNIEnv* env, jclass clazz) {
    jint j_shaders = 0;

    if (EmulationSession::GetInstance().IsRunning()) {
        j_shaders = EmulationSession::GetInstance().ShadersBuilding();
    }

    return j_shaders;
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getCpuBackend(JNIEnv* env, jclass clazz) {
    if (Settings::IsNceEnabled()) {
        return Common::Android::ToJString(env, "NCE");
    }

    return Common::Android::ToJString(env, "JIT");
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getGpuDriver(JNIEnv* env, jobject jobj) {
    return Common::Android::ToJString(
        env, EmulationSession::GetInstance().System().GPU().Renderer().GetDeviceVendor());
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getCpuSummary(JNIEnv* env, jobject /*jobj*/) {
    get_arm_cpu_name();
    constexpr const char* CPUINFO_PATH = "/proc/cpuinfo";

    auto trim = [](std::string& s) {
        const auto start = s.find_first_not_of(" \t\r\n");
        const auto end = s.find_last_not_of(" \t\r\n");
        s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    };

    auto to_lower = [](std::string s) {
        for (auto& c : s) c = std::tolower(c);
        return s;
    };

    try {
        std::string result;
        std::pair<u32, std::string> pretty_cpus = get_pretty_cpus();
        u32 threads = pretty_cpus.first;
        std::string cpus = pretty_cpus.second;

        fmt::format_to(std::back_inserter(result), "CPUs: {}\n{} Threads",
                       cpus, threads);

        FILE* f = std::fopen(CPUINFO_PATH, "r");
        if (!f) return Common::Android::ToJString(env, result);

        char buf[512];

        if (f) {
            std::set<std::string> feature_set;
            while (std::fgets(buf, sizeof(buf), f)) {
                std::string line(buf);
                if (line.find("Features") == 0 || line.find("features") == 0) {
                    auto pos = line.find(':');
                    if (pos != std::string::npos) {
                        std::string feat_line = line.substr(pos + 1);
                        trim(feat_line);

                        std::istringstream iss(feat_line);
                        std::string feature;
                        while (iss >> feature) {
                            feature_set.insert(to_lower(feature));
                        }
                    }
                }
            }
            std::fclose(f);

            bool has_neon = feature_set.count("neon") || feature_set.count("asimd");
            bool has_fp = feature_set.count("fp") || feature_set.count("vfp");
            bool has_sve = feature_set.count("sve");
            bool has_sve2 = feature_set.count("sve2");
            bool has_crypto = feature_set.count("aes") || feature_set.count("sha1") ||
                             feature_set.count("sha2") || feature_set.count("pmull");
            bool has_dotprod = feature_set.count("asimddp") || feature_set.count("dotprod");
            bool has_i8mm = feature_set.count("i8mm");
            bool has_bf16 = feature_set.count("bf16");
            bool has_atomics = feature_set.count("atomics") || feature_set.count("lse");

            std::string features;
            if (has_neon || has_fp) {
                features += "NEON";
                if (has_dotprod) features += "+DP";
                if (has_i8mm) features += "+I8MM";
                if (has_bf16) features += "+BF16";
            }

            if (has_sve) {
                if (!features.empty()) features += " | ";
                features += "SVE";
                if (has_sve2) features += "2";
            }

            if (has_crypto) {
                if (!features.empty()) features += " | ";
                features += "Crypto";
            }

            if (has_atomics) {
                if (!features.empty()) features += " | ";
                features += "LSE";
            }

            if (!features.empty()) {
                result += "\nFeatures: " + features;
            }
        }

        fmt::format_to(std::back_inserter(result), "\nLLVM CPU: {}", fallback_cpu_detection());

        return Common::Android::ToJString(env, result);
    } catch (...) {
        return Common::Android::ToJString(env, "Unknown");
    }
}


namespace {
constexpr u32 VENDOR_QUALCOMM = 0x5143;
constexpr u32 VENDOR_ARM = 0x13B5;

VkPhysicalDeviceProperties GetVulkanDeviceProperties() {
    Common::DynamicLibrary library;
    if (!library.Open("libvulkan.so")) {
        return {};
    }

    Vulkan::vk::InstanceDispatch dld;
    // TODO: warn the user that Vulkan is unavailable rather than hard crash
    const auto instance = Vulkan::CreateInstance(library, dld, VK_API_VERSION_1_1);
    const auto physical_devices = instance.EnumeratePhysicalDevices();
    if (physical_devices.empty()) {
        return {};
    }

    const Vulkan::vk::PhysicalDevice physical_device(physical_devices[0], dld);
    return physical_device.GetProperties();
}
} // namespace

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getVulkanDriverVersion(JNIEnv* env, jobject jobj) {
    try {
        const auto props = GetVulkanDeviceProperties();
        if (props.deviceID == 0) {
            return Common::Android::ToJString(env, "N/A");
        }

        const u32 driver_version = props.driverVersion;
        const u32 vendor_id = props.vendorID;

        if (driver_version == 0) {
            return Common::Android::ToJString(env, "N/A");
        }

        std::string version_str;

        if (vendor_id == VENDOR_QUALCOMM) {
            const u32 major = (driver_version >> 24) << 2;
            const u32 minor = (driver_version >> 12) & 0xFFF;
            const u32 patch = driver_version & 0xFFF;
            version_str = fmt::format("{}.{}.{}", major, minor, patch);
        }
        else if (vendor_id == VENDOR_ARM) {
            u32 major = VK_API_VERSION_MAJOR(driver_version);
            u32 minor = VK_API_VERSION_MINOR(driver_version);
            u32 patch = VK_API_VERSION_PATCH(driver_version);

            // ARM custom encoding for newer drivers
            if (major > 10) {
                major = (driver_version >> 22) & 0x3FF;
                minor = (driver_version >> 12) & 0x3FF;
                patch = driver_version & 0xFFF;
            }
            version_str = fmt::format("{}.{}.{}", major, minor, patch);
        }
        // Standard Vulkan version encoding for other vendors
        else {
            const u32 major = VK_API_VERSION_MAJOR(driver_version);
            const u32 minor = VK_API_VERSION_MINOR(driver_version);
            const u32 patch = VK_API_VERSION_PATCH(driver_version);
            version_str = fmt::format("{}.{}.{}", major, minor, patch);
        }

        return Common::Android::ToJString(env, version_str);
    } catch (...) {
        return Common::Android::ToJString(env, "N/A");
    }
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getVulkanApiVersion(JNIEnv* env, jobject jobj) {
    try {
        const auto props = GetVulkanDeviceProperties();
        if (props.deviceID == 0) {
            return Common::Android::ToJString(env, "N/A");
        }

        const u32 api_version = props.apiVersion;
        const u32 major = VK_API_VERSION_MAJOR(api_version);
        const u32 minor = VK_API_VERSION_MINOR(api_version);
        const u32 patch = VK_API_VERSION_PATCH(api_version);
        const u32 variant = VK_API_VERSION_VARIANT(api_version);

        // Include variant if non-zero (rare on Android)
        const std::string version_str = variant > 0
            ? fmt::format("{}.{}.{}.{}", variant, major, minor, patch)
            : fmt::format("{}.{}.{}", major, minor, patch);

        return Common::Android::ToJString(env, version_str);
    } catch (...) {
        return Common::Android::ToJString(env, "N/A");
    }
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getGpuModel(JNIEnv* env, jobject jobj) {
    const auto props = GetVulkanDeviceProperties();
    if (props.deviceID == 0) {
        return Common::Android::ToJString(env, "Unknown");
    }

    return Common::Android::ToJString(env, props.deviceName);
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_applySettings(JNIEnv* env, jobject jobj) {
    EmulationSession::GetInstance().System().ApplySettings();
    EmulationSession::GetInstance().System().HIDCore().ReloadInputDevices();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_logSettings(JNIEnv* env, jobject jobj) {
    Settings::LogSettings();
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_getDebugKnobAt(JNIEnv* env, jobject jobj, jint index) {
    return static_cast<jboolean>(Settings::getDebugKnobAt(static_cast<u8>(index)));
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_setTurboSpeedLimit(JNIEnv *env, jobject jobj, jboolean enabled) {
    if (enabled) {
        Settings::values.use_speed_limit.SetValue(true);
        Settings::SetSpeedMode(Settings::SpeedMode::Turbo);
    } else {
        Settings::SetSpeedMode(Settings::SpeedMode::Standard);
    }
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_setSlowSpeedLimit(JNIEnv *env, jobject jobj, jboolean enabled) {
    if (enabled) {
        Settings::values.use_speed_limit.SetValue(true);
        Settings::SetSpeedMode(Settings::SpeedMode::Slow);
    } else {
        Settings::SetSpeedMode(Settings::SpeedMode::Standard);
    }
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_setStandardSpeedLimit(JNIEnv *env, jobject jobj, jboolean enabled) {
    Settings::values.use_speed_limit.SetValue(enabled);
    if (enabled) {
        Settings::SetSpeedMode(Settings::SpeedMode::Standard);
    }
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_isTurboMode(JNIEnv *env, jobject jobj) {
    return Settings::values.current_speed_mode.GetValue() == Settings::SpeedMode::Turbo;
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_isSlowMode(JNIEnv *env, jobject jobj) {
    return Settings::values.current_speed_mode.GetValue() == Settings::SpeedMode::Slow;
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_run(JNIEnv* env, jobject jobj, jstring j_path,
                                               jint j_program_index,
                                               jboolean j_frontend_initiated) {
    const std::string path = Common::Android::GetJString(env, j_path);

    const Core::SystemResultStatus result{
        RunEmulation(path, j_program_index, j_frontend_initiated)};
    if (result != Core::SystemResultStatus::Success) {
        env->CallStaticVoidMethod(Common::Android::GetNativeLibraryClass(),
                                  Common::Android::GetExitEmulationActivity(),
                                  static_cast<int>(result));
    }
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_logDeviceInfo(JNIEnv* env, jclass clazz) {
    LOG_INFO(Frontend, "eden Version: {}-{}", Common::g_scm_branch, Common::g_scm_desc);
    LOG_INFO(Frontend, "Host OS: Android API level {}", android_get_device_api_level());
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_submitInlineKeyboardText(JNIEnv* env, jclass clazz,
                                                                    jstring j_text) {
    const std::u16string input = Common::UTF8ToUTF16(Common::Android::GetJString(env, j_text));
    EmulationSession::GetInstance().SoftwareKeyboard()->SubmitInlineKeyboardText(input);
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_submitInlineKeyboardInput(JNIEnv* env, jclass clazz,
                                                                     jint j_key_code) {
    EmulationSession::GetInstance().SoftwareKeyboard()->SubmitInlineKeyboardInput(j_key_code);
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_initializeEmptyUserDirectory(JNIEnv* env,
                                                                        jobject instance) {
    const auto nand_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir);
    auto vfs_nand_dir = EmulationSession::GetInstance().System().GetFilesystem()->OpenDirectory(
        Common::FS::PathToUTF8String(nand_dir), FileSys::OpenMode::Read);

    const auto user_id = EmulationSession::GetInstance().System().GetProfileManager().GetUser(
        static_cast<std::size_t>(0));
    ASSERT(user_id);

    const auto user_save_data_path = FileSys::SaveDataFactory::GetFullPath(
        {}, vfs_nand_dir, FileSys::SaveDataSpaceId::User, FileSys::SaveDataType::Account, 1,
        user_id->AsU128(), 0);

    const auto full_path = Common::FS::ConcatPathSafe(nand_dir, user_save_data_path);
    if (!Common::FS::CreateParentDirs(full_path)) {
        LOG_WARNING(Frontend, "Failed to create full path of the default user's save directory");
    }
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_playTimeManagerInit(JNIEnv* env, jobject obj) {
    // for some reason the full user directory isnt initialized in Android, so we need to create it
    const auto play_time_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::PlayTimeDir);
    if (!Common::FS::IsDir(play_time_dir) && !Common::FS::CreateDir(play_time_dir))
        LOG_WARNING(Frontend, "Failed to create play time directory");

    play_time_manager = std::make_unique<PlayTime::PlayTimeManager>();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_playTimeManagerStart(JNIEnv* env, jobject obj) {
    if (play_time_manager) {
        play_time_manager->SetProgramId(EmulationSession::GetInstance().System().GetApplicationProcessProgramID());
        play_time_manager->Start();
    }
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_playTimeManagerStop(JNIEnv* env, jobject obj) {
    if (play_time_manager)
        play_time_manager->Stop();
}

jlong Java_org_yuzu_yuzu_1emu_NativeLibrary_playTimeManagerGetPlayTime(JNIEnv* env, jobject obj, jstring jprogramId) {
    if (play_time_manager) {
        u64 program_id = EmulationSession::GetProgramId(env, jprogramId);
        return play_time_manager->GetPlayTime(program_id);
    }
    return 0UL;
}

jlong Java_org_yuzu_yuzu_1emu_NativeLibrary_playTimeManagerGetCurrentTitleId(JNIEnv* env,
                                                                             jobject obj) {
    return EmulationSession::GetInstance().System().GetApplicationProcessProgramID();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_playTimeManagerResetProgramPlayTime(JNIEnv* env, jobject obj,
                                                                jstring jprogramId) {
    if (play_time_manager) {
        u64 program_id = EmulationSession::GetProgramId(env, jprogramId);
        play_time_manager->ResetProgramPlayTime(program_id);
    }
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_playTimeManagerSetPlayTime(JNIEnv* env, jobject obj,
                                                                jstring jprogramId, jlong playTimeSeconds) {
    if (play_time_manager) {
        u64 program_id = EmulationSession::GetProgramId(env, jprogramId);
        play_time_manager->SetPlayTime(program_id, u64(playTimeSeconds));
    }
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getAppletLaunchPath(JNIEnv* env, jclass clazz,
                                                                  jlong jid) {
    auto bis_system =
        EmulationSession::GetInstance().System().GetFileSystemController().GetSystemNANDContents();
    if (!bis_system) {
        return Common::Android::ToJString(env, "");
    }

    auto applet_nca =
        bis_system->GetEntry(static_cast<u64>(jid), FileSys::ContentRecordType::Program);
    if (!applet_nca) {
        return Common::Android::ToJString(env, "");
    }

    return Common::Android::ToJString(env, applet_nca->GetFullPath());
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_setCurrentAppletId(JNIEnv* env, jclass clazz,
                                                              jint jappletId) {
    EmulationSession::GetInstance().SetAppletId(jappletId);
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_setCabinetMode(JNIEnv* env, jclass clazz,
                                                          jint jcabinetMode) {
    EmulationSession::GetInstance().System().GetFrontendAppletHolder().SetCabinetMode(
        static_cast<Service::NFP::CabinetMode>(jcabinetMode));
}

bool isFirmwarePresent() {
    return FirmwareManager::CheckFirmwarePresence(EmulationSession::GetInstance().System());
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_isFirmwareAvailable(JNIEnv* env, jclass clazz) {
    return isFirmwarePresent();
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_firmwareVersion(JNIEnv* env, jclass clazz) {
    const auto pair = FirmwareManager::GetFirmwareVersion(EmulationSession::GetInstance().System());
    const auto firmware_data = pair.first;
    const auto result = pair.second;

    if (result.IsError() || !isFirmwarePresent()) {
        return Common::Android::ToJString(env, "N/A");
    }

    const std::string display_version(firmware_data.display_version.data());
    const std::string display_title(firmware_data.display_title.data());

    LOG_INFO(Frontend, "Installed firmware: {}", display_title);

    return Common::Android::ToJString(env, display_version);
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_gameRequiresFirmware(JNIEnv* env, jclass clazz, jstring jprogramId) {
    auto program_id = EmulationSession::GetProgramId(env, jprogramId);

    return FirmwareManager::GameRequiresFirmware(program_id);
}

jint Java_org_yuzu_yuzu_1emu_NativeLibrary_installKeys(JNIEnv* env, jclass clazz, jstring jpath, jstring jext) {
    const auto path = Common::Android::GetJString(env, jpath);
    const auto ext = Common::Android::GetJString(env, jext);

    return static_cast<int>(FirmwareManager::InstallKeys(path, ext));
}

jobjectArray Java_org_yuzu_yuzu_1emu_NativeLibrary_getPatchesForFile(JNIEnv* env, jobject jobj,
                                                                     jstring jpath,
                                                                     jstring jprogramId) {
    const auto path = Common::Android::GetJString(env, jpath);
    const auto vFile =
        Core::GetGameFileFromPath(EmulationSession::GetInstance().System().GetFilesystem(), path);
    if (vFile == nullptr) {
        return nullptr;
    }

    auto& system = EmulationSession::GetInstance().System();
    auto program_id = EmulationSession::GetProgramId(env, jprogramId);
    const FileSys::PatchManager pm{program_id, system.GetFileSystemController(),
                                   system.GetContentProvider()};
    const auto loader = Loader::GetLoader(system, vFile);

    FileSys::VirtualFile update_raw;
    loader->ReadUpdateRaw(update_raw);

    auto patches = pm.GetPatches(update_raw);
    jobjectArray jpatchArray =
        env->NewObjectArray(patches.size(), Common::Android::GetPatchClass(), nullptr);
    int i = 0;
    for (const auto& patch : patches) {
        jobject jpatch = env->NewObject(
            Common::Android::GetPatchClass(), Common::Android::GetPatchConstructor(), patch.enabled,
            Common::Android::ToJString(env, patch.name),
            Common::Android::ToJString(env, patch.version), static_cast<jint>(patch.type),
            Common::Android::ToJString(env, std::to_string(patch.program_id)),
            Common::Android::ToJString(env, std::to_string(patch.title_id)),
            static_cast<jlong>(patch.numeric_version));
        env->SetObjectArrayElement(jpatchArray, i, jpatch);
        ++i;
    }
    return jpatchArray;
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_removeUpdate(JNIEnv* env, jobject jobj,
                                                        jstring jprogramId) {
    auto program_id = EmulationSession::GetProgramId(env, jprogramId);
    ContentManager::RemoveUpdate(EmulationSession::GetInstance().System().GetFileSystemController(),
                                 program_id);
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_removeDLC(JNIEnv* env, jobject jobj,
                                                     jstring jprogramId) {
    auto program_id = EmulationSession::GetProgramId(env, jprogramId);
    ContentManager::RemoveAllDLC(EmulationSession::GetInstance().System(), program_id);
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_removeMod(JNIEnv* env, jobject jobj, jstring jprogramId,
                                                     jstring jname) {
    auto program_id = EmulationSession::GetProgramId(env, jprogramId);
    ContentManager::RemoveMod(EmulationSession::GetInstance().System().GetFileSystemController(),
                              program_id, Common::Android::GetJString(env, jname));
}

jobjectArray Java_org_yuzu_yuzu_1emu_NativeLibrary_verifyInstalledContents(JNIEnv* env,
                                                                           jobject jobj,
                                                                           jobject jcallback) {
    auto jlambdaClass = env->GetObjectClass(jcallback);
    auto jlambdaInvokeMethod = env->GetMethodID(
        jlambdaClass, "invoke", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    const auto callback = [env, jcallback, jlambdaInvokeMethod](size_t max, size_t progress) {
        auto jwasCancelled = env->CallObjectMethod(jcallback, jlambdaInvokeMethod,
                                                   Common::Android::ToJDouble(env, max),
                                                   Common::Android::ToJDouble(env, progress));
        return Common::Android::GetJBoolean(env, jwasCancelled);
    };

    auto& session = EmulationSession::GetInstance();
    std::vector<std::string> result = ContentManager::VerifyInstalledContents(
        session.System(), *session.GetContentProvider(), callback);
    jobjectArray jresult = env->NewObjectArray(result.size(), Common::Android::GetStringClass(),
                                               Common::Android::ToJString(env, ""));
    for (size_t i = 0; i < result.size(); ++i) {
        env->SetObjectArrayElement(jresult, i, Common::Android::ToJString(env, result[i]));
    }
    return jresult;
}

jint Java_org_yuzu_yuzu_1emu_NativeLibrary_verifyGameContents(JNIEnv* env, jobject jobj,
                                                              jstring jpath, jobject jcallback) {
    auto jlambdaClass = env->GetObjectClass(jcallback);
    auto jlambdaInvokeMethod = env->GetMethodID(
        jlambdaClass, "invoke", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    const auto callback = [env, jcallback, jlambdaInvokeMethod](size_t max, size_t progress) {
        auto jwasCancelled = env->CallObjectMethod(jcallback, jlambdaInvokeMethod,
                                                   Common::Android::ToJDouble(env, max),
                                                   Common::Android::ToJDouble(env, progress));
        return Common::Android::GetJBoolean(env, jwasCancelled);
    };
    auto& session = EmulationSession::GetInstance();
    return static_cast<jint>(ContentManager::VerifyGameContents(
        session.System(), Common::Android::GetJString(env, jpath), callback));
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getSavePath(JNIEnv* env, jobject jobj,
                                                          jstring jprogramId) {
    auto program_id = EmulationSession::GetProgramId(env, jprogramId);
    if (program_id == 0) {
        return Common::Android::ToJString(env, "");
    }

    auto& system = EmulationSession::GetInstance().System();

    Service::Account::ProfileManager manager;
    // TODO: Pass in a selected user once we get the relevant UI working
    const auto user_id = manager.GetUser(static_cast<std::size_t>(0));
    ASSERT(user_id);

    const auto saveDir = Common::FS::GetEdenPath(Common::FS::EdenPath::SaveDir);
    auto vfsSaveDir = system.GetFilesystem()->OpenDirectory(Common::FS::PathToUTF8String(saveDir),
                                                            FileSys::OpenMode::Read);

    const auto user_save_data_path = FileSys::SaveDataFactory::GetFullPath(
        {}, vfsSaveDir, FileSys::SaveDataSpaceId::User, FileSys::SaveDataType::Account, program_id,
        user_id->AsU128(), 0);
    return Common::Android::ToJString(env, user_save_data_path);
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getDefaultProfileSaveDataRoot(JNIEnv* env,
                                                                            jobject jobj,
                                                                            jboolean jfuture) {
    Service::Account::ProfileManager manager;
    // TODO: Pass in a selected user once we get the relevant UI working
    const auto user_id = manager.GetUser(static_cast<std::size_t>(0));
    ASSERT(user_id);

    const auto user_save_data_root =
        FileSys::SaveDataFactory::GetUserGameSaveDataRoot(user_id->AsU128(), jfuture);
    return Common::Android::ToJString(env, user_save_data_root);
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_addFileToFilesystemProvider(JNIEnv* env, jobject jobj,
                                                                       jstring jpath) {
    EmulationSession::GetInstance().ConfigureFilesystemProvider(
        Common::Android::GetJString(env, jpath));
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_clearFilesystemProvider(JNIEnv* env, jobject jobj) {
    EmulationSession::GetInstance().GetContentProvider()->ClearAllEntries();
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_areKeysPresent(JNIEnv* env, jobject jobj) {
    auto& system = EmulationSession::GetInstance().System();
    system.GetFileSystemController().CreateFactories(*system.GetFilesystem());
    return ContentManager::AreKeysPresent();
}

jint Java_org_yuzu_yuzu_1emu_NativeLibrary_getVirtualAmiiboState(JNIEnv* env, jobject jobj) {
    if (!EmulationSession::GetInstance().IsRunning()) {
        return static_cast<jint>(InputCommon::VirtualAmiibo::State::Disabled);
    }

    auto* virtual_amiibo =
        EmulationSession::GetInstance().GetInputSubsystem().GetVirtualAmiibo();
    if (virtual_amiibo == nullptr) {
        return static_cast<jint>(InputCommon::VirtualAmiibo::State::Disabled);
    }

    return static_cast<jint>(virtual_amiibo->GetCurrentState());
}

jint Java_org_yuzu_yuzu_1emu_NativeLibrary_loadAmiibo(JNIEnv* env, jobject jobj,
                                                      jbyteArray jdata) {
    if (!EmulationSession::GetInstance().IsRunning() || jdata == nullptr) {
        return static_cast<jint>(InputCommon::VirtualAmiibo::Info::WrongDeviceState);
    }

    auto* virtual_amiibo =
        EmulationSession::GetInstance().GetInputSubsystem().GetVirtualAmiibo();
    if (virtual_amiibo == nullptr) {
        return static_cast<jint>(InputCommon::VirtualAmiibo::Info::Unknown);
    }

    const jsize length = env->GetArrayLength(jdata);
    std::vector<u8> bytes(static_cast<std::size_t>(length));
    if (length > 0) {
        env->GetByteArrayRegion(jdata, 0, length,
                                reinterpret_cast<jbyte*>(bytes.data()));
    }

    const auto info =
        virtual_amiibo->LoadAmiibo(std::span<u8>(bytes.data(), bytes.size()));
    return static_cast<jint>(info);
}

JNIEXPORT void JNICALL
Java_org_yuzu_yuzu_1emu_NativeLibrary_initMultiplayer(
        JNIEnv* env, [[maybe_unused]] jobject obj) {
    if (multiplayer) {
        return;
    }

    announce_multiplayer_session = std::make_shared<Core::AnnounceMultiplayerSession>();

    multiplayer = std::make_unique<AndroidMultiplayer>(s_instance.System(), announce_multiplayer_session);
    multiplayer->NetworkInit();
}

JNIEXPORT jobjectArray JNICALL
Java_org_yuzu_yuzu_1emu_network_NetPlayManager_netPlayGetPublicRooms(
        JNIEnv *env, [[maybe_unused]] jobject obj) {
    return Common::Android::ToJStringArray(env, multiplayer->NetPlayGetPublicRooms());
}

JNIEXPORT jint JNICALL Java_org_yuzu_yuzu_1emu_network_NetPlayManager_netPlayCreateRoom(
        JNIEnv* env, [[maybe_unused]] jobject obj, jstring ipaddress, jint port,
        jstring username, jstring preferredGameName, jlong preferredGameId, jstring password,
        jstring room_name, jint max_players, jboolean isPublic) {
    return static_cast<jint>(
            multiplayer->NetPlayCreateRoom(Common::Android::GetJString(env, ipaddress), port,
                              Common::Android::GetJString(env, username), Common::Android::GetJString(env, preferredGameName),
                              preferredGameId,Common::Android::GetJString(env, password),
                              Common::Android::GetJString(env, room_name), max_players, isPublic));
}

JNIEXPORT jint JNICALL Java_org_yuzu_yuzu_1emu_network_NetPlayManager_netPlayJoinRoom(
        JNIEnv* env, [[maybe_unused]] jobject obj, jstring ipaddress, jint port,
        jstring username, jstring password) {
    return static_cast<jint>(
            multiplayer->NetPlayJoinRoom(Common::Android::GetJString(env, ipaddress), port,
                            Common::Android::GetJString(env, username), Common::Android::GetJString(env, password)));
}

JNIEXPORT jobjectArray JNICALL
Java_org_yuzu_yuzu_1emu_network_NetPlayManager_netPlayRoomInfo(
        JNIEnv* env, [[maybe_unused]] jobject obj) {
    return Common::Android::ToJStringArray(env, multiplayer->NetPlayRoomInfo());
}

JNIEXPORT jboolean JNICALL
Java_org_yuzu_yuzu_1emu_network_NetPlayManager_netPlayIsJoined(
        [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    return multiplayer->NetPlayIsJoined();
}

JNIEXPORT jboolean JNICALL
Java_org_yuzu_yuzu_1emu_network_NetPlayManager_netPlayIsHostedRoom(
        [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    return multiplayer->NetPlayIsHostedRoom();
}

JNIEXPORT void JNICALL
Java_org_yuzu_yuzu_1emu_network_NetPlayManager_netPlaySendMessage(
        JNIEnv* env, [[maybe_unused]] jobject obj, jstring msg) {
    multiplayer->NetPlaySendMessage(Common::Android::GetJString(env, msg));
}

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_network_NetPlayManager_netPlayKickUser(
        JNIEnv* env, [[maybe_unused]] jobject obj, jstring username) {
    multiplayer->NetPlayKickUser(Common::Android::GetJString(env, username));
}

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_network_NetPlayManager_netPlayLeaveRoom(
        [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    multiplayer->NetPlayLeaveRoom();
}

JNIEXPORT jboolean JNICALL
Java_org_yuzu_yuzu_1emu_network_NetPlayManager_netPlayIsModerator(
        [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    return multiplayer->NetPlayIsModerator();
}

JNIEXPORT jobjectArray JNICALL
Java_org_yuzu_yuzu_1emu_network_NetPlayManager_netPlayGetBanList(
        JNIEnv* env, [[maybe_unused]] jobject obj) {
    return Common::Android::ToJStringArray(env, multiplayer->NetPlayGetBanList());
}

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_network_NetPlayManager_netPlayBanUser(
        JNIEnv* env, [[maybe_unused]] jobject obj, jstring username) {
    multiplayer->NetPlayBanUser(Common::Android::GetJString(env, username));
}

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_network_NetPlayManager_netPlayUnbanUser(
        JNIEnv* env, [[maybe_unused]] jobject obj, jstring username) {
    multiplayer->NetPlayUnbanUser(Common::Android::GetJString(env, username));
}

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_updatePowerState(
        JNIEnv* env,
        jobject,
        jint percentage,
        jboolean isCharging,
        jboolean hasBattery) {

    g_battery_percentage.store(percentage, std::memory_order_relaxed);
    g_is_charging.store(isCharging, std::memory_order_relaxed);
    g_has_battery.store(hasBattery, std::memory_order_relaxed);
}

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_isUpdateCheckerEnabled(
        JNIEnv* env,
        jobject obj) {
#ifdef ENABLE_UPDATE_CHECKER
    return JNI_TRUE;
#else
    return JNI_FALSE;
#endif
    }

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_isNightlyBuild(
    JNIEnv* env,
    jobject obj) {
#ifdef NIGHTLY_BUILD
    return JNI_TRUE;
#else
    return JNI_FALSE;
#endif
}

#ifdef ENABLE_UPDATE_CHECKER


JNIEXPORT jobjectArray JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_checkForUpdate(
        JNIEnv* env,
        jobject obj) {
    std::optional<UpdateChecker::Update> release = UpdateChecker::GetUpdate();
    if (!release) return nullptr;

    const std::string tag = release->tag;
    const std::string name = release->name;

    jobjectArray result = env->NewObjectArray(2, env->FindClass("java/lang/String"), nullptr);

    const jstring jtag = env->NewStringUTF(tag.c_str());
    const jstring jname = env->NewStringUTF(name.c_str());

    env->SetObjectArrayElement(result, 0, jtag);
    env->SetObjectArrayElement(result, 1, jname);
    env->DeleteLocalRef(jtag);
    env->DeleteLocalRef(jname);

    return result;
}

JNIEXPORT jstring JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_getUpdateUrl(
        JNIEnv* env,
        jobject obj,
        jstring version) {
    const char* version_str = env->GetStringUTFChars(version, nullptr);
    const std::string url = fmt::format("{}/{}/releases/tag/{}",
        std::string{Common::g_build_auto_update_website},
        std::string{Common::g_build_auto_update_repo},
        version_str);
    env->ReleaseStringUTFChars(version, version_str);
    return env->NewStringUTF(url.c_str());
}

JNIEXPORT jstring JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_getUpdateApkUrl(
        JNIEnv* env,
        jobject obj,
        jstring tag,
        jstring artifact,
        jstring packageId) {
    const char* version_str = env->GetStringUTFChars(tag, nullptr);
    const char* artifact_str = env->GetStringUTFChars(artifact, nullptr);
    const char* package_id_str = env->GetStringUTFChars(packageId, nullptr);

    std::string variant;
    std::string package_id(package_id_str);

    if (package_id.find("dev.legacy.eden_emulator") != std::string::npos) {
        variant = "legacy";
    } else if (package_id.find("com.miHoYo.Yuanshen") != std::string::npos) {
        variant = "optimized";
    } else {
#ifdef ARCHITECTURE_arm64
        variant = "standard";
#else
        variant = "chromeos";
#endif
    }

    const std::string apk_filename = fmt::format("Eden-Android-{}-{}.apk", artifact_str, variant);
    const std::string url = fmt::format("{}/{}/releases/download/{}/{}",
        std::string{Common::g_build_auto_update_website},
        std::string{Common::g_build_auto_update_repo},
        version_str,
        apk_filename);

    env->ReleaseStringUTFChars(tag, version_str);
    env->ReleaseStringUTFChars(artifact, artifact_str);
    env->ReleaseStringUTFChars(packageId, package_id_str);
    return env->NewStringUTF(url.c_str());
}
#endif

JNIEXPORT jstring JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_getBuildVersion(
        JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    return env->NewStringUTF(Common::g_build_version);
}

JNIEXPORT jobjectArray JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_getAllUsers(
        JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    auto& manager = EmulationSession::GetInstance().System().GetProfileManager();

    manager.ResetUserSaveFile();

    if (manager.GetUserCount() == 0) {
        manager.CreateNewUser(Common::UUID::MakeRandom(), "Eden");
        manager.WriteUserSaveFile();
    }

    const auto& users = manager.GetAllUsers();

    jclass string_class = env->FindClass("java/lang/String");
    if (!string_class) {
        return env->NewObjectArray(0, env->FindClass("java/lang/Object"), nullptr);
    }

    jsize valid_count = 0;
    for (const auto& user : users) {
        if (user.IsValid()) {
            valid_count++;
        }
    }

    jobjectArray result = env->NewObjectArray(valid_count, string_class, nullptr);
    if (!result) {
        return env->NewObjectArray(0, string_class, nullptr);
    }

    // fill array sequentially with only valid users
    jsize array_index = 0;
    for (const auto& user : users) {
        if (user.IsValid()) {
            jstring uuid_str = env->NewStringUTF(user.FormattedString().c_str());
            if (uuid_str) {
                env->SetObjectArrayElement(result, array_index++, uuid_str);
            }
        }
    }

    return result;
}

JNIEXPORT jstring JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_getUserUsername(
        JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jstring juuid) {
    auto& manager = EmulationSession::GetInstance().System().GetProfileManager();
    const auto uuid_string = Common::Android::GetJString(env, juuid);
    const auto uuid = Common::UUID{uuid_string};

    Service::Account::ProfileBase profile{};
    if (!manager.GetProfileBase(uuid, profile)) {
        jstring result = env->NewStringUTF("");
        return result ? result : env->NewStringUTF("");
    }

    const auto text = Common::StringFromFixedZeroTerminatedBuffer(
        reinterpret_cast<const char*>(profile.username.data()), profile.username.size());
    jstring result = env->NewStringUTF(text.c_str());
    return result ? result : env->NewStringUTF("");
}

JNIEXPORT jlong JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_getUserCount(
        JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    auto& manager = EmulationSession::GetInstance().System().GetProfileManager();
    return static_cast<jlong>(manager.GetUserCount());
}

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_canCreateUser(
        JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    auto& manager = EmulationSession::GetInstance().System().GetProfileManager();
    return manager.CanSystemRegisterUser();
}

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_createUser(
        JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jstring juuid,
        jstring jusername) {
    auto& manager = EmulationSession::GetInstance().System().GetProfileManager();
    const auto uuid_string = Common::Android::GetJString(env, juuid);
    const auto username = Common::Android::GetJString(env, jusername);
    const auto uuid = Common::UUID{uuid_string};

    const auto result = manager.CreateNewUser(uuid, username);
    if (result.IsSuccess()) {
        manager.WriteUserSaveFile();
        return true;
    }
    return false;
}

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_updateUserUsername(
        JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jstring juuid,
        jstring jusername) {
    auto& manager = EmulationSession::GetInstance().System().GetProfileManager();
    const auto uuid_string = Common::Android::GetJString(env, juuid);
    const auto username = Common::Android::GetJString(env, jusername);
    const auto uuid = Common::UUID{uuid_string};

    Service::Account::ProfileBase profile{};
    if (!manager.GetProfileBase(uuid, profile)) {
        return false;
    }

    std::fill(profile.username.begin(), profile.username.end(), '\0');
    std::copy(username.begin(), username.end(), profile.username.begin());

    if (manager.SetProfileBase(uuid, profile)) {
        manager.WriteUserSaveFile();
        return true;
    }
    return false;
}

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_removeUser(
        JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jstring juuid) {
    auto& manager = EmulationSession::GetInstance().System().GetProfileManager();
    const auto uuid_string = Common::Android::GetJString(env, juuid);
    const auto uuid = Common::UUID{uuid_string};

    const auto user_index = manager.GetUserIndex(uuid);
    if (!user_index) {
        return false;
    }

    if (Settings::values.current_user.GetValue() == static_cast<s32>(*user_index)) {
        Settings::values.current_user = 0;
    }

    if (manager.RemoveUser(uuid)) {
        manager.WriteUserSaveFile();
        manager.ResetUserSaveFile();
        return true;
    }
    return false;
}

JNIEXPORT jstring JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_getCurrentUser(
        JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    auto& manager = EmulationSession::GetInstance().System().GetProfileManager();
    const auto user_id = manager.GetUser(Settings::values.current_user.GetValue());
    if (!user_id) {
        jstring result = env->NewStringUTF("");
        return result ? result : env->NewStringUTF("");
    }
    jstring result = env->NewStringUTF(user_id->FormattedString().c_str());
    return result ? result : env->NewStringUTF("");
}

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_setCurrentUser(
        JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jstring juuid) {
    auto& manager = EmulationSession::GetInstance().System().GetProfileManager();
    const auto uuid_string = Common::Android::GetJString(env, juuid);
    const auto uuid = Common::UUID{uuid_string};

    const auto index = manager.GetUserIndex(uuid);
    if (index) {
        Settings::values.current_user = static_cast<s32>(*index);
        return true;
    }
    return false;
}

JNIEXPORT jstring JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_getUserImagePath(
        JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jstring juuid) {
    const auto uuid_string = Common::Android::GetJString(env, juuid);
    const auto uuid = Common::UUID{uuid_string};

    const auto path = Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir) /
        fmt::format("system/save/8000000000000010/su/avators/{}.jpg", uuid.FormattedString());

    jstring result = Common::Android::ToJString(env, Common::FS::PathToUTF8String(path));
    return result ? result : env->NewStringUTF("");
}

JNIEXPORT jboolean JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_saveUserImage(
        JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jstring juuid,
        jstring jimagePath) {
    const auto uuid_string = Common::Android::GetJString(env, juuid);
    const auto uuid = Common::UUID{uuid_string};
    const auto image_source = Common::Android::GetJString(env, jimagePath);

    const auto dest_path = Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir) /
        fmt::format("system/save/8000000000000010/su/avators/{}.jpg", uuid.FormattedString());

    const auto dest_dir = dest_path.parent_path();
    if (!Common::FS::CreateDirs(dest_dir)) {
        return false;
    }

    try {
        std::filesystem::copy_file(image_source, dest_path,
            std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR(Common_Filesystem, "Failed to copy image file: {}", e.what());
        return false;
    }
}

JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_reloadProfiles(
        JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    auto& manager = EmulationSession::GetInstance().System().GetProfileManager();
    manager.ResetUserSaveFile();

    // create a default user if non exist
    if (manager.GetUserCount() == 0) {
        manager.CreateNewUser(Common::UUID::MakeRandom(), "Eden");
        manager.WriteUserSaveFile();
    }

    LOG_INFO(Service_ACC, "Profile manager reloaded, user count: {}", manager.GetUserCount());
}

//  for firmware avatar images
static std::vector<uint8_t> DecompressYaz0(const FileSys::VirtualFile& file) {
    if (!file) {
        return std::vector<uint8_t>();
    }

    uint32_t magic{};
    file->ReadObject(&magic, 0);
    if (magic != Common::MakeMagic('Y', 'a', 'z', '0')) {
        return std::vector<uint8_t>();
    }

    uint32_t decoded_length{};
    file->ReadObject(&decoded_length, 4);
    decoded_length = Common::swap32(decoded_length);

    std::size_t input_size = file->GetSize() - 16;
    std::vector<uint8_t> input(input_size);
    file->ReadBytes(input.data(), input_size, 16);

    uint32_t input_offset{};
    uint32_t output_offset{};
    std::vector<uint8_t> output(decoded_length);

    uint16_t mask{};
    uint8_t header{};

    while (output_offset < decoded_length) {
        if ((mask >>= 1) == 0) {
            if (input_offset >= input.size()) break;
            header = input[input_offset++];
            mask = 0x80;
        }

        if ((header & mask) != 0) {
            if (output_offset >= output.size() || input_offset >= input.size()) {
                break;
            }
            output[output_offset++] = input[input_offset++];
        } else {
            if (input_offset + 1 >= input.size()) break;
            uint8_t byte1 = input[input_offset++];
            uint8_t byte2 = input[input_offset++];

            uint32_t dist = ((byte1 & 0xF) << 8) | byte2;
            uint32_t position = output_offset - (dist + 1);

            uint32_t length = byte1 >> 4;
            if (length == 0) {
                if (input_offset >= input.size()) break;
                length = static_cast<uint32_t>(input[input_offset++]) + 0x12;
            } else {
                length += 2;
            }

            for (uint32_t i = 0; i < length && output_offset < decoded_length; ++i) {
                output[output_offset++] = output[position++];
            }
        }
    }

    return output;
}

static FileSys::VirtualDir GetFirmwareAvatarDirectory() {
    constexpr u64 AvatarImageDataId = 0x010000000000080AULL;

    auto* bis_system = EmulationSession::GetInstance().System().GetFileSystemController().GetSystemNANDContents();
    if (!bis_system) {
        return nullptr;
    }

    const auto nca = bis_system->GetEntry(AvatarImageDataId, FileSys::ContentRecordType::Data);
    if (!nca) {
        return nullptr;
    }

    const auto romfs = nca->GetRomFS();
    if (!romfs) {
        return nullptr;
    }

    const auto extracted = FileSys::ExtractRomFS(romfs);
    if (!extracted) {
        return nullptr;
    }

    return extracted->GetSubdirectory("chara");
}

JNIEXPORT jint JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_getFirmwareAvatarCount(
        JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    const auto chara_dir = GetFirmwareAvatarDirectory();
    if (!chara_dir) {
        return 0;
    }

    int count = 0;
    for (const auto& item : chara_dir->GetFiles()) {
        if (item->GetExtension() == "szs") {
            count++;
        }
    }
    return count;
}

JNIEXPORT jbyteArray JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_getFirmwareAvatarImage(
        JNIEnv* env,
        [[maybe_unused]] jobject obj,
        jint index) {
    const auto chara_dir = GetFirmwareAvatarDirectory();
    if (!chara_dir) {
        return nullptr;
    }

    int current_index = 0;
    for (const auto& item : chara_dir->GetFiles()) {
        if (item->GetExtension() != "szs") {
            continue;
        }

        if (current_index == index) {
            auto image_data = DecompressYaz0(item);
            if (image_data.empty()) {
                return nullptr;
            }

            jbyteArray result = env->NewByteArray(image_data.size());
            if (result) {
                env->SetByteArrayRegion(result, 0, image_data.size(),
                    reinterpret_cast<const jbyte*>(image_data.data()));
            }
            return result;
        }
        current_index++;
    }

    return nullptr;
}

JNIEXPORT jbyteArray JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_getDefaultAccountBackupJpeg(
        JNIEnv* env,
        [[maybe_unused]] jobject obj) {
    jbyteArray result = env->NewByteArray(Core::Constants::ACCOUNT_BACKUP_JPEG.size());
    if (result) {
        env->SetByteArrayRegion(result, 0, Core::Constants::ACCOUNT_BACKUP_JPEG.size(),
            reinterpret_cast<const jbyte*>(Core::Constants::ACCOUNT_BACKUP_JPEG.data()));
    }
    return result;
}

} // extern "C"
