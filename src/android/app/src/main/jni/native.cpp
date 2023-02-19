// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <codecvt>
#include <locale>
#include <string>
#include <string_view>
#include <dlfcn.h>

#include <adrenotools/driver.h>

#include <android/api-level.h>
#include <android/native_window_jni.h>

#include "common/detached_tasks.h"
#include "common/dynamic_library.h"
#include "common/fs/path_util.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/cpu_manager.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/vfs_real.h"
#include "core/hid/hid_core.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/loader.h"
#include "core/perf_stats.h"
#include "jni/config.h"
#include "jni/emu_window/emu_window.h"
#include "jni/id_cache.h"
#include "video_core/rasterizer_interface.h"

namespace {

class EmulationSession final {
public:
    EmulationSession() {
        m_system.Initialize();
        m_vfs = std::make_shared<FileSys::RealVfsFilesystem>();
    }

    ~EmulationSession() = default;

    static EmulationSession& GetInstance() {
        return s_instance;
    }

    const Core::System& System() const {
        return m_system;
    }

    Core::System& System() {
        return m_system;
    }

    const EmuWindow_Android& Window() const {
        return *m_window;
    }

    EmuWindow_Android& Window() {
        return *m_window;
    }

    ANativeWindow* NativeWindow() const {
        return m_native_window;
    }

    void SetNativeWindow(ANativeWindow* m_native_window_) {
        m_native_window = m_native_window_;
    }

    void InitializeGpuDriver(const std::string& hook_lib_dir, const std::string& custom_driver_dir,
                             const std::string& custom_driver_name,
                             const std::string& file_redirect_dir) {
        void* handle{};

        // Try to load a custom driver.
        if (custom_driver_name.size()) {
            handle = adrenotools_open_libvulkan(
                RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM | ADRENOTOOLS_DRIVER_FILE_REDIRECT, nullptr,
                hook_lib_dir.c_str(), custom_driver_dir.c_str(), custom_driver_name.c_str(),
                file_redirect_dir.c_str(), nullptr);
        }

        // Try to load the system driver.
        if (!handle) {
            handle = adrenotools_open_libvulkan(RTLD_NOW, ADRENOTOOLS_DRIVER_FILE_REDIRECT, nullptr,
                                                hook_lib_dir.c_str(), nullptr, nullptr,
                                                file_redirect_dir.c_str(), nullptr);
        }

        m_vulkan_library = std::make_shared<Common::DynamicLibrary>(handle);
    }

    bool IsRunning() const {
        std::scoped_lock lock(m_mutex);
        return m_is_running;
    }

    const Core::PerfStatsResults& PerfStats() const {
        std::scoped_lock m_perf_stats_lock(m_perf_stats_mutex);
        return m_perf_stats;
    }

    void SurfaceChanged() {
        if (!IsRunning()) {
            return;
        }
        m_window->OnSurfaceChanged(m_native_window);
    }

    Core::SystemResultStatus InitializeEmulation(const std::string& filepath) {
        std::scoped_lock lock(m_mutex);

        // Loads the configuration.
        Config{};

        // Create the render window.
        m_window = std::make_unique<EmuWindow_Android>(&m_input_subsystem, m_native_window,
                                                       m_vulkan_library);

        // Initialize system.
        m_system.SetShuttingDown(false);
        m_system.Initialize();
        m_system.ApplySettings();
        m_system.HIDCore().ReloadInputDevices();
        m_system.SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
        m_system.SetFilesystem(std::make_shared<FileSys::RealVfsFilesystem>());
        m_system.GetFileSystemController().CreateFactories(*m_system.GetFilesystem());

        // Load the ROM.
        m_load_result = m_system.Load(EmulationSession::GetInstance().Window(), filepath);
        if (m_load_result != Core::SystemResultStatus::Success) {
            return m_load_result;
        }

        // Complete initialization.
        m_system.GPU().Start();
        m_system.GetCpuManager().OnGpuReady();
        m_system.RegisterExitCallback([&] { HaltEmulation(); });

        return Core::SystemResultStatus::Success;
    }

    void ShutdownEmulation() {
        std::scoped_lock lock(m_mutex);

        m_is_running = false;

        // Unload user input.
        m_system.HIDCore().UnloadInputDevices();

        // Shutdown the main emulated process
        if (m_load_result == Core::SystemResultStatus::Success) {
            m_system.DetachDebugger();
            m_system.ShutdownMainProcess();
            m_detached_tasks.WaitForAllTasks();
            m_load_result = Core::SystemResultStatus::ErrorNotInitialized;
        }

        // Tear down the render window.
        m_window.reset();
    }

    void PauseEmulation() {
        std::scoped_lock lock(m_mutex);
        m_system.Pause();
    }

    void UnPauseEmulation() {
        std::scoped_lock lock(m_mutex);
        m_system.Run();
    }

    void HaltEmulation() {
        std::scoped_lock lock(m_mutex);
        m_is_running = false;
        m_cv.notify_one();
    }

    void RunEmulation() {
        {
            std::scoped_lock lock(m_mutex);
            m_is_running = true;
        }

        void(m_system.Run());

        if (m_system.DebuggerEnabled()) {
            m_system.InitializeDebugger();
        }

        while (true) {
            {
                std::unique_lock lock(m_mutex);
                if (m_cv.wait_for(lock, std::chrono::milliseconds(800),
                                  [&]() { return !m_is_running; })) {
                    // Emulation halted.
                    break;
                }
            }
            {
                // Refresh performance stats.
                std::scoped_lock m_perf_stats_lock(m_perf_stats_mutex);
                m_perf_stats = m_system.GetAndResetPerfStats();
            }
        }
    }

    std::string GetRomTitle(const std::string& path) {
        return GetRomMetadata(path).title;
    }

    std::vector<u8> GetRomIcon(const std::string& path) {
        return GetRomMetadata(path).icon;
    }

    void ResetRomMetadata() {
        m_rom_metadata_cache.clear();
    }

private:
    struct RomMetadata {
        std::string title;
        std::vector<u8> icon;
    };

    RomMetadata GetRomMetadata(const std::string& path) {
        if (auto search = m_rom_metadata_cache.find(path); search != m_rom_metadata_cache.end()) {
            return search->second;
        }

        return CacheRomMetadata(path);
    }

    RomMetadata CacheRomMetadata(const std::string& path) {
        const auto file = Core::GetGameFileFromPath(m_vfs, path);
        const auto loader = Loader::GetLoader(EmulationSession::GetInstance().System(), file, 0, 0);

        RomMetadata entry;
        loader->ReadTitle(entry.title);
        loader->ReadIcon(entry.icon);

        m_rom_metadata_cache[path] = entry;

        return entry;
    }

private:
    static EmulationSession s_instance;

    // Frontend management
    std::unordered_map<std::string, RomMetadata> m_rom_metadata_cache;

    // Window management
    std::unique_ptr<EmuWindow_Android> m_window;
    ANativeWindow* m_native_window{};

    // Core emulation
    Core::System m_system;
    InputCommon::InputSubsystem m_input_subsystem;
    Common::DetachedTasks m_detached_tasks;
    Core::PerfStatsResults m_perf_stats{};
    std::shared_ptr<FileSys::RealVfsFilesystem> m_vfs;
    Core::SystemResultStatus m_load_result{Core::SystemResultStatus::ErrorNotInitialized};
    bool m_is_running{};

    // GPU driver parameters
    std::shared_ptr<Common::DynamicLibrary> m_vulkan_library;

    // Synchronization
    std::condition_variable_any m_cv;
    mutable std::mutex m_perf_stats_mutex;
    mutable std::mutex m_mutex;
};

/*static*/ EmulationSession EmulationSession::s_instance;

std::string UTF16ToUTF8(std::u16string_view input) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.to_bytes(input.data(), input.data() + input.size());
}

std::string GetJString(JNIEnv* env, jstring jstr) {
    if (!jstr) {
        return {};
    }

    const jchar* jchars = env->GetStringChars(jstr, nullptr);
    const jsize length = env->GetStringLength(jstr);
    const std::u16string_view string_view(reinterpret_cast<const char16_t*>(jchars), length);
    const std::string converted_string = UTF16ToUTF8(string_view);
    env->ReleaseStringChars(jstr, jchars);

    return converted_string;
}

} // Anonymous namespace

static Core::SystemResultStatus RunEmulation(const std::string& filepath) {
    Common::Log::Initialize();
    Common::Log::SetColorConsoleBackendEnabled(true);
    Common::Log::Start();

    MicroProfileOnThreadCreate("EmuThread");
    SCOPE_EXIT({ MicroProfileShutdown(); });

    LOG_INFO(Frontend, "starting");

    if (filepath.empty()) {
        LOG_CRITICAL(Frontend, "failed to load: filepath empty!");
        return Core::SystemResultStatus::ErrorLoader;
    }

    SCOPE_EXIT({ EmulationSession::GetInstance().ShutdownEmulation(); });

    const auto result = EmulationSession::GetInstance().InitializeEmulation(filepath);
    if (result != Core::SystemResultStatus::Success) {
        return result;
    }

    EmulationSession::GetInstance().RunEmulation();

    return Core::SystemResultStatus::Success;
}

extern "C" {

void Java_org_yuzu_yuzu_1emu_NativeLibrary_SurfaceChanged(JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz,
                                                          jobject surf) {
    EmulationSession::GetInstance().SetNativeWindow(ANativeWindow_fromSurface(env, surf));
    EmulationSession::GetInstance().SurfaceChanged();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_SurfaceDestroyed(JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz) {
    ANativeWindow_release(EmulationSession::GetInstance().NativeWindow());
    EmulationSession::GetInstance().SetNativeWindow(nullptr);
    EmulationSession::GetInstance().SurfaceChanged();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_DoFrame(JNIEnv* env, [[maybe_unused]] jclass clazz) {}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_NotifyOrientationChange(JNIEnv* env,
                                                                   [[maybe_unused]] jclass clazz,
                                                                   jint layout_option,
                                                                   jint rotation) {}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_SetAppDirectory(JNIEnv* env,
                                                           [[maybe_unused]] jclass clazz,
                                                           jstring j_directory) {
    Common::FS::SetAppDirectory(GetJString(env, j_directory));
}

void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_InitializeGpuDriver(
    JNIEnv* env, [[maybe_unused]] jclass clazz, jstring hook_lib_dir, jstring custom_driver_dir,
    jstring custom_driver_name, jstring file_redirect_dir) {
    EmulationSession::GetInstance().InitializeGpuDriver(
        GetJString(env, hook_lib_dir), GetJString(env, custom_driver_dir),
        GetJString(env, custom_driver_name), GetJString(env, file_redirect_dir));
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_ReloadKeys(JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz) {
    Core::Crypto::KeyManager::Instance().ReloadKeys();
    return static_cast<jboolean>(Core::Crypto::KeyManager::Instance().AreKeysLoaded());
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_UnPauseEmulation([[maybe_unused]] JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz) {
    EmulationSession::GetInstance().UnPauseEmulation();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_PauseEmulation([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz) {
    EmulationSession::GetInstance().PauseEmulation();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_StopEmulation([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jclass clazz) {
    EmulationSession::GetInstance().HaltEmulation();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_ResetRomMetadata([[maybe_unused]] JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz) {
    EmulationSession::GetInstance().ResetRomMetadata();
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_IsRunning([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jclass clazz) {
    return static_cast<jboolean>(EmulationSession::GetInstance().IsRunning());
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadButtonEvent([[maybe_unused]] JNIEnv* env,
                                                                    [[maybe_unused]] jclass clazz,
                                                                    [[maybe_unused]] jint j_device,
                                                                    jint j_button, jint action) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnGamepadButtonEvent(j_device, j_button,
                                                                      action != 0);
    }
    return static_cast<jboolean>(true);
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadJoystickEvent([[maybe_unused]] JNIEnv* env,
                                                                      [[maybe_unused]] jclass clazz,
                                                                      jint j_device, jint stick_id,
                                                                      jfloat x, jfloat y) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnGamepadJoystickEvent(j_device, stick_id, x, y);
    }
    return static_cast<jboolean>(true);
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadMotionEvent(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint j_device,
    jlong delta_timestamp, jfloat gyro_x, jfloat gyro_y, jfloat gyro_z, jfloat accel_x,
    jfloat accel_y, jfloat accel_z) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnGamepadMotionEvent(
            j_device, delta_timestamp, gyro_x, gyro_y, gyro_z, accel_x, accel_y, accel_z);
    }
    return static_cast<jboolean>(true);
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_onTouchPressed([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz, jint id,
                                                          jfloat x, jfloat y) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnTouchPressed(id, x, y);
    }
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_onTouchMoved([[maybe_unused]] JNIEnv* env,
                                                        [[maybe_unused]] jclass clazz, jint id,
                                                        jfloat x, jfloat y) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnTouchMoved(id, x, y);
    }
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_onTouchReleased([[maybe_unused]] JNIEnv* env,
                                                           [[maybe_unused]] jclass clazz, jint id) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnTouchReleased(id);
    }
}

jbyteArray Java_org_yuzu_yuzu_1emu_NativeLibrary_GetIcon([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jclass clazz,
                                                         [[maybe_unused]] jstring j_filename) {
    auto icon_data = EmulationSession::GetInstance().GetRomIcon(GetJString(env, j_filename));
    jbyteArray icon = env->NewByteArray(static_cast<jsize>(icon_data.size()));
    env->SetByteArrayRegion(icon, 0, env->GetArrayLength(icon),
                            reinterpret_cast<jbyte*>(icon_data.data()));
    return icon;
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_GetTitle([[maybe_unused]] JNIEnv* env,
                                                       [[maybe_unused]] jclass clazz,
                                                       [[maybe_unused]] jstring j_filename) {
    auto title = EmulationSession::GetInstance().GetRomTitle(GetJString(env, j_filename));
    return env->NewStringUTF(title.c_str());
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_GetDescription([[maybe_unused]] JNIEnv* env,
                                                             [[maybe_unused]] jclass clazz,
                                                             jstring j_filename) {
    return j_filename;
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_GetGameId([[maybe_unused]] JNIEnv* env,
                                                        [[maybe_unused]] jclass clazz,
                                                        jstring j_filename) {
    return j_filename;
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_GetRegions([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jclass clazz,
                                                         [[maybe_unused]] jstring j_filename) {
    return env->NewStringUTF("");
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_GetCompany([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jclass clazz,
                                                         [[maybe_unused]] jstring j_filename) {
    return env->NewStringUTF("");
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_GetGitRevision([[maybe_unused]] JNIEnv* env,
                                                             [[maybe_unused]] jclass clazz) {
    return {};
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_CreateConfigFile
    [[maybe_unused]] (JNIEnv* env, [[maybe_unused]] jclass clazz) {
    Config{};
}

jint Java_org_yuzu_yuzu_1emu_NativeLibrary_DefaultCPUCore([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz) {
    return {};
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_Run__Ljava_lang_String_2Ljava_lang_String_2Z(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, [[maybe_unused]] jstring j_file,
    [[maybe_unused]] jstring j_savestate, [[maybe_unused]] jboolean j_delete_savestate) {}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_ReloadSettings([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz) {
    Config{};
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_GetUserSetting([[maybe_unused]] JNIEnv* env,
                                                             [[maybe_unused]] jclass clazz,
                                                             jstring j_game_id, jstring j_section,
                                                             jstring j_key) {
    std::string_view game_id = env->GetStringUTFChars(j_game_id, 0);
    std::string_view section = env->GetStringUTFChars(j_section, 0);
    std::string_view key = env->GetStringUTFChars(j_key, 0);

    env->ReleaseStringUTFChars(j_game_id, game_id.data());
    env->ReleaseStringUTFChars(j_section, section.data());
    env->ReleaseStringUTFChars(j_key, key.data());

    return env->NewStringUTF("");
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_SetUserSetting([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz,
                                                          jstring j_game_id, jstring j_section,
                                                          jstring j_key, jstring j_value) {
    std::string_view game_id = env->GetStringUTFChars(j_game_id, 0);
    std::string_view section = env->GetStringUTFChars(j_section, 0);
    std::string_view key = env->GetStringUTFChars(j_key, 0);
    std::string_view value = env->GetStringUTFChars(j_value, 0);

    env->ReleaseStringUTFChars(j_game_id, game_id.data());
    env->ReleaseStringUTFChars(j_section, section.data());
    env->ReleaseStringUTFChars(j_key, key.data());
    env->ReleaseStringUTFChars(j_value, value.data());
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_InitGameIni([[maybe_unused]] JNIEnv* env,
                                                       [[maybe_unused]] jclass clazz,
                                                       jstring j_game_id) {
    std::string_view game_id = env->GetStringUTFChars(j_game_id, 0);

    env->ReleaseStringUTFChars(j_game_id, game_id.data());
}

jdoubleArray Java_org_yuzu_yuzu_1emu_NativeLibrary_GetPerfStats([[maybe_unused]] JNIEnv* env,
                                                                [[maybe_unused]] jclass clazz) {
    jdoubleArray j_stats = env->NewDoubleArray(4);

    if (EmulationSession::GetInstance().IsRunning()) {
        const auto results = EmulationSession::GetInstance().PerfStats();

        // Converting the structure into an array makes it easier to pass it to the frontend
        double stats[4] = {results.system_fps, results.average_game_fps, results.frametime,
                           results.emulation_speed};

        env->SetDoubleArrayRegion(j_stats, 0, 4, stats);
    }

    return j_stats;
}

void Java_org_yuzu_yuzu_1emu_utils_DirectoryInitialization_SetSysDirectory(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jstring j_path) {}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_Run__Ljava_lang_String_2([[maybe_unused]] JNIEnv* env,
                                                                    [[maybe_unused]] jclass clazz,
                                                                    jstring j_path) {
    const std::string path = GetJString(env, j_path);

    const Core::SystemResultStatus result{RunEmulation(path)};
    if (result != Core::SystemResultStatus::Success) {
        env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(),
                                  IDCache::GetExitEmulationActivity(), static_cast<int>(result));
    }
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_LogDeviceInfo([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jclass clazz) {
    LOG_INFO(Frontend, "yuzu Version: {}-{}", Common::g_scm_branch, Common::g_scm_desc);
    LOG_INFO(Frontend, "Host OS: Android API level {}", android_get_device_api_level());
}

} // extern "C"
