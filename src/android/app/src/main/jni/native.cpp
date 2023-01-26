#include <codecvt>
#include <locale>
#include <string>
#include <string_view>

#include <android/api-level.h>
#include <android/native_window_jni.h>

#include "common/detached_tasks.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/cpu_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/vfs_real.h"
#include "core/hid/hid_core.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/perf_stats.h"
#include "jni/config.h"
#include "jni/emu_window/emu_window.h"
#include "jni/id_cache.h"
#include "video_core/rasterizer_interface.h"

namespace {

class EmulationSession final {
public:
    EmulationSession() = default;
    ~EmulationSession() = default;

    static EmulationSession& GetInstance() {
        return s_instance;
    }

    const Core::System& System() const {
        return system;
    }

    Core::System& System() {
        return system;
    }

    const EmuWindow_Android& Window() const {
        return *window;
    }

    EmuWindow_Android& Window() {
        return *window;
    }

    ANativeWindow* NativeWindow() const {
        return native_window;
    }

    void SetNativeWindow(ANativeWindow* native_window_) {
        native_window = native_window_;
    }

    bool IsRunning() const {
        std::scoped_lock lock(mutex);
        return is_running;
    }

    const Core::PerfStatsResults& PerfStats() const {
        std::scoped_lock perf_stats_lock(perf_stats_mutex);
        return perf_stats;
    }

    void SurfaceChanged() {
        if (!IsRunning()) {
            return;
        }
        window->OnSurfaceChanged(native_window);
    }

    Core::SystemResultStatus InitializeEmulation(const std::string& filepath) {
        std::scoped_lock lock(mutex);

        // Loads the configuration.
        Config{};

        // Create the render window.
        window = std::make_unique<EmuWindow_Android>(&input_subsystem, native_window);

        // Initialize system.
        system.SetShuttingDown(false);
        system.Initialize();
        system.ApplySettings();
        system.HIDCore().ReloadInputDevices();
        system.SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
        system.SetFilesystem(std::make_shared<FileSys::RealVfsFilesystem>());
        system.GetFileSystemController().CreateFactories(*system.GetFilesystem());

        // Load the ROM.
        const Core::SystemResultStatus load_result{
            system.Load(EmulationSession::GetInstance().Window(), filepath)};
        if (load_result != Core::SystemResultStatus::Success) {
            return load_result;
        }

        // Complete initialization.
        system.GPU().Start();
        system.GetCpuManager().OnGpuReady();
        system.RegisterExitCallback([&] { HaltEmulation(); });

        return Core::SystemResultStatus::Success;
    }

    void ShutdownEmulation() {
        std::scoped_lock lock(mutex);

        // Unload user input.
        system.HIDCore().UnloadInputDevices();

        // Shutdown the main emulated process
        system.DetachDebugger();
        system.ShutdownMainProcess();
        detached_tasks.WaitForAllTasks();

        // Tear down the render window.
        window.reset();
    }

    void HaltEmulation() {
        is_running = false;
        cv.notify_one();
    }

    void RunEmulation() {
        {
            std::scoped_lock lock(mutex);
            is_running = true;
        }

        void(system.Run());

        if (system.DebuggerEnabled()) {
            system.InitializeDebugger();
        }

        while (true) {
            {
                std::unique_lock lock(mutex);
                if (cv.wait_for(lock, std::chrono::seconds(1), [&]() { return !is_running; })) {
                    // Emulation halted.
                    break;
                }
            }
            {
                // Refresh performance stats.
                std::scoped_lock perf_stats_lock(perf_stats_mutex);
                perf_stats = system.GetAndResetPerfStats();
            }
        }
    }

private:
    static EmulationSession s_instance;

    ANativeWindow* native_window{};

    InputCommon::InputSubsystem input_subsystem;
    Common::DetachedTasks detached_tasks;
    Core::System system;

    Core::PerfStatsResults perf_stats{};

    std::unique_ptr<EmuWindow_Android> window;
    std::condition_variable_any cv;
    bool is_running{};

    mutable std::mutex perf_stats_mutex;
    mutable std::mutex mutex;
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

    const auto result = EmulationSession::GetInstance().InitializeEmulation(filepath);
    if (result != Core::SystemResultStatus::Success) {
        return result;
    }

    EmulationSession::GetInstance().RunEmulation();
    EmulationSession::GetInstance().ShutdownEmulation();

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

void Java_org_yuzu_yuzu_1emu_NativeLibrary_SetUserDirectory([[maybe_unused]] JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz,
                                                            [[maybe_unused]] jstring j_directory) {}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_UnPauseEmulation([[maybe_unused]] JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz) {}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_PauseEmulation([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz) {}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_StopEmulation([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jclass clazz) {
    EmulationSession::GetInstance().HaltEmulation();
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_IsRunning([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jclass clazz) {
    return static_cast<jboolean>(EmulationSession::GetInstance().IsRunning());
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadEvent([[maybe_unused]] JNIEnv* env,
                                                              [[maybe_unused]] jclass clazz,
                                                              [[maybe_unused]] jstring j_device,
                                                              jint j_button, jint action) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnGamepadEvent(j_button, action != 0);
    }
    return static_cast<jboolean>(true);
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadMoveEvent([[maybe_unused]] JNIEnv* env,
                                                                  [[maybe_unused]] jclass clazz,
                                                                  jstring j_device, jint axis,
                                                                  jfloat x, jfloat y) {
    // Clamp joystick movement to supported minimum and maximum.
    x = std::clamp(x, -1.f, 1.f);
    y = std::clamp(-y, -1.f, 1.f);

    // Clamp the input to a circle.
    float r = x * x + y * y;
    if (r > 1.0f) {
        r = std::sqrt(r);
        x /= r;
        y /= r;
    }

    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnGamepadMoveEvent(x, y);
    }
    return static_cast<jboolean>(false);
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadAxisEvent([[maybe_unused]] JNIEnv* env,
                                                                  [[maybe_unused]] jclass clazz,
                                                                  jstring j_device, jint axis_id,
                                                                  jfloat axis_val) {
    return {};
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_onTouchEvent([[maybe_unused]] JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz, jfloat x,
                                                            jfloat y, jboolean pressed) {
    if (EmulationSession::GetInstance().IsRunning()) {
        return static_cast<jboolean>(
            EmulationSession::GetInstance().Window().OnTouchEvent(x, y, pressed));
    }
    return static_cast<jboolean>(false);
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_onTouchMoved([[maybe_unused]] JNIEnv* env,
                                                        [[maybe_unused]] jclass clazz, jfloat x,
                                                        jfloat y) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnTouchMoved(x, y);
    }
}

jintArray Java_org_yuzu_yuzu_1emu_NativeLibrary_GetIcon([[maybe_unused]] JNIEnv* env,
                                                        [[maybe_unused]] jclass clazz,
                                                        [[maybe_unused]] jstring j_file) {
    return {};
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_GetTitle([[maybe_unused]] JNIEnv* env,
                                                       [[maybe_unused]] jclass clazz,
                                                       [[maybe_unused]] jstring j_filename) {
    return env->NewStringUTF("");
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
