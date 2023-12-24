// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include <common/fs/fs_util.h>
#include <jni.h>

#include "android_config.h"
#include "android_settings.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "frontend_common/config.h"
#include "jni/android_common/android_common.h"
#include "jni/id_cache.h"
#include "native.h"

std::unique_ptr<AndroidConfig> global_config;
std::unique_ptr<AndroidConfig> per_game_config;

template <typename T>
Settings::Setting<T>* getSetting(JNIEnv* env, jstring jkey) {
    auto key = GetJString(env, jkey);
    auto basic_setting = Settings::values.linkage.by_key[key];
    if (basic_setting != 0) {
        return static_cast<Settings::Setting<T>*>(basic_setting);
    }
    auto basic_android_setting = AndroidSettings::values.linkage.by_key[key];
    if (basic_android_setting != 0) {
        return static_cast<Settings::Setting<T>*>(basic_android_setting);
    }
    LOG_ERROR(Frontend, "[Android Native] Could not find setting - {}", key);
    return nullptr;
}

extern "C" {

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_initializeGlobalConfig(JNIEnv* env, jobject obj) {
    global_config = std::make_unique<AndroidConfig>();
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_unloadGlobalConfig(JNIEnv* env, jobject obj) {
    global_config.reset();
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_reloadGlobalConfig(JNIEnv* env, jobject obj) {
    global_config->AndroidConfig::ReloadAllValues();
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_saveGlobalConfig(JNIEnv* env, jobject obj) {
    global_config->AndroidConfig::SaveAllValues();
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_initializePerGameConfig(JNIEnv* env, jobject obj,
                                                                        jstring jprogramId,
                                                                        jstring jfileName) {
    auto program_id = EmulationSession::GetProgramId(env, jprogramId);
    auto file_name = GetJString(env, jfileName);
    const auto config_file_name = program_id == 0 ? file_name : fmt::format("{:016X}", program_id);
    per_game_config =
        std::make_unique<AndroidConfig>(config_file_name, Config::ConfigType::PerGameConfig);
}

jboolean Java_org_yuzu_yuzu_1emu_utils_NativeConfig_isPerGameConfigLoaded(JNIEnv* env,
                                                                          jobject obj) {
    return per_game_config != nullptr;
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_savePerGameConfig(JNIEnv* env, jobject obj) {
    per_game_config->AndroidConfig::SaveAllValues();
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_unloadPerGameConfig(JNIEnv* env, jobject obj) {
    per_game_config.reset();
}

jboolean Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getBoolean(JNIEnv* env, jobject obj,
                                                               jstring jkey, jboolean needGlobal) {
    auto setting = getSetting<bool>(env, jkey);
    if (setting == nullptr) {
        return false;
    }
    return setting->GetValue(static_cast<bool>(needGlobal));
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setBoolean(JNIEnv* env, jobject obj, jstring jkey,
                                                           jboolean value) {
    auto setting = getSetting<bool>(env, jkey);
    if (setting == nullptr) {
        return;
    }
    setting->SetValue(static_cast<bool>(value));
}

jbyte Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getByte(JNIEnv* env, jobject obj, jstring jkey,
                                                         jboolean needGlobal) {
    auto setting = getSetting<u8>(env, jkey);
    if (setting == nullptr) {
        return -1;
    }
    return setting->GetValue(static_cast<bool>(needGlobal));
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setByte(JNIEnv* env, jobject obj, jstring jkey,
                                                        jbyte value) {
    auto setting = getSetting<u8>(env, jkey);
    if (setting == nullptr) {
        return;
    }
    setting->SetValue(value);
}

jshort Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getShort(JNIEnv* env, jobject obj, jstring jkey,
                                                           jboolean needGlobal) {
    auto setting = getSetting<u16>(env, jkey);
    if (setting == nullptr) {
        return -1;
    }
    return setting->GetValue(static_cast<bool>(needGlobal));
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setShort(JNIEnv* env, jobject obj, jstring jkey,
                                                         jshort value) {
    auto setting = getSetting<u16>(env, jkey);
    if (setting == nullptr) {
        return;
    }
    setting->SetValue(value);
}

jint Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getInt(JNIEnv* env, jobject obj, jstring jkey,
                                                       jboolean needGlobal) {
    auto setting = getSetting<int>(env, jkey);
    if (setting == nullptr) {
        return -1;
    }
    return setting->GetValue(needGlobal);
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setInt(JNIEnv* env, jobject obj, jstring jkey,
                                                       jint value) {
    auto setting = getSetting<int>(env, jkey);
    if (setting == nullptr) {
        return;
    }
    setting->SetValue(value);
}

jfloat Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getFloat(JNIEnv* env, jobject obj, jstring jkey,
                                                           jboolean needGlobal) {
    auto setting = getSetting<float>(env, jkey);
    if (setting == nullptr) {
        return -1;
    }
    return setting->GetValue(static_cast<bool>(needGlobal));
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setFloat(JNIEnv* env, jobject obj, jstring jkey,
                                                         jfloat value) {
    auto setting = getSetting<float>(env, jkey);
    if (setting == nullptr) {
        return;
    }
    setting->SetValue(value);
}

jlong Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getLong(JNIEnv* env, jobject obj, jstring jkey,
                                                         jboolean needGlobal) {
    auto setting = getSetting<s64>(env, jkey);
    if (setting == nullptr) {
        return -1;
    }
    return setting->GetValue(static_cast<bool>(needGlobal));
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setLong(JNIEnv* env, jobject obj, jstring jkey,
                                                        jlong value) {
    auto setting = getSetting<long>(env, jkey);
    if (setting == nullptr) {
        return;
    }
    setting->SetValue(value);
}

jstring Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getString(JNIEnv* env, jobject obj, jstring jkey,
                                                             jboolean needGlobal) {
    auto setting = getSetting<std::string>(env, jkey);
    if (setting == nullptr) {
        return ToJString(env, "");
    }
    return ToJString(env, setting->GetValue(static_cast<bool>(needGlobal)));
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setString(JNIEnv* env, jobject obj, jstring jkey,
                                                          jstring value) {
    auto setting = getSetting<std::string>(env, jkey);
    if (setting == nullptr) {
        return;
    }

    setting->SetValue(GetJString(env, value));
}

jboolean Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getIsRuntimeModifiable(JNIEnv* env, jobject obj,
                                                                           jstring jkey) {
    auto setting = getSetting<std::string>(env, jkey);
    if (setting != nullptr) {
        return setting->RuntimeModfiable();
    }
    return true;
}

jstring Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getPairedSettingKey(JNIEnv* env, jobject obj,
                                                                       jstring jkey) {
    auto setting = getSetting<std::string>(env, jkey);
    if (setting == nullptr) {
        return ToJString(env, "");
    }
    if (setting->PairedSetting() == nullptr) {
        return ToJString(env, "");
    }

    return ToJString(env, setting->PairedSetting()->GetLabel());
}

jboolean Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getIsSwitchable(JNIEnv* env, jobject obj,
                                                                    jstring jkey) {
    auto setting = getSetting<std::string>(env, jkey);
    if (setting != nullptr) {
        return setting->Switchable();
    }
    return false;
}

jboolean Java_org_yuzu_yuzu_1emu_utils_NativeConfig_usingGlobal(JNIEnv* env, jobject obj,
                                                                jstring jkey) {
    auto setting = getSetting<std::string>(env, jkey);
    if (setting != nullptr) {
        return setting->UsingGlobal();
    }
    return true;
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setGlobal(JNIEnv* env, jobject obj, jstring jkey,
                                                          jboolean global) {
    auto setting = getSetting<std::string>(env, jkey);
    if (setting != nullptr) {
        setting->SetGlobal(static_cast<bool>(global));
    }
}

jboolean Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getIsSaveable(JNIEnv* env, jobject obj,
                                                                  jstring jkey) {
    auto setting = getSetting<std::string>(env, jkey);
    if (setting != nullptr) {
        return setting->Save();
    }
    return false;
}

jstring Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getDefaultToString(JNIEnv* env, jobject obj,
                                                                      jstring jkey) {
    auto setting = getSetting<std::string>(env, jkey);
    if (setting != nullptr) {
        return ToJString(env, setting->DefaultToString());
    }
    return ToJString(env, "");
}

jobjectArray Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getGameDirs(JNIEnv* env, jobject obj) {
    jclass gameDirClass = IDCache::GetGameDirClass();
    jmethodID gameDirConstructor = IDCache::GetGameDirConstructor();
    jobjectArray jgameDirArray =
        env->NewObjectArray(AndroidSettings::values.game_dirs.size(), gameDirClass, nullptr);
    for (size_t i = 0; i < AndroidSettings::values.game_dirs.size(); ++i) {
        jobject jgameDir =
            env->NewObject(gameDirClass, gameDirConstructor,
                           ToJString(env, AndroidSettings::values.game_dirs[i].path),
                           static_cast<jboolean>(AndroidSettings::values.game_dirs[i].deep_scan));
        env->SetObjectArrayElement(jgameDirArray, i, jgameDir);
    }
    return jgameDirArray;
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setGameDirs(JNIEnv* env, jobject obj,
                                                            jobjectArray gameDirs) {
    AndroidSettings::values.game_dirs.clear();
    int size = env->GetArrayLength(gameDirs);

    if (size == 0) {
        return;
    }

    jobject dir = env->GetObjectArrayElement(gameDirs, 0);
    jclass gameDirClass = IDCache::GetGameDirClass();
    jfieldID uriStringField = env->GetFieldID(gameDirClass, "uriString", "Ljava/lang/String;");
    jfieldID deepScanBooleanField = env->GetFieldID(gameDirClass, "deepScan", "Z");
    for (int i = 0; i < size; ++i) {
        dir = env->GetObjectArrayElement(gameDirs, i);
        jstring juriString = static_cast<jstring>(env->GetObjectField(dir, uriStringField));
        jboolean jdeepScanBoolean = env->GetBooleanField(dir, deepScanBooleanField);
        std::string uriString = GetJString(env, juriString);
        AndroidSettings::values.game_dirs.push_back(
            AndroidSettings::GameDir{uriString, static_cast<bool>(jdeepScanBoolean)});
    }
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_addGameDir(JNIEnv* env, jobject obj,
                                                           jobject gameDir) {
    jclass gameDirClass = IDCache::GetGameDirClass();
    jfieldID uriStringField = env->GetFieldID(gameDirClass, "uriString", "Ljava/lang/String;");
    jfieldID deepScanBooleanField = env->GetFieldID(gameDirClass, "deepScan", "Z");

    jstring juriString = static_cast<jstring>(env->GetObjectField(gameDir, uriStringField));
    jboolean jdeepScanBoolean = env->GetBooleanField(gameDir, deepScanBooleanField);
    std::string uriString = GetJString(env, juriString);
    AndroidSettings::values.game_dirs.push_back(
        AndroidSettings::GameDir{uriString, static_cast<bool>(jdeepScanBoolean)});
}

jobjectArray Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getDisabledAddons(JNIEnv* env, jobject obj,
                                                                          jstring jprogramId) {
    auto program_id = EmulationSession::GetProgramId(env, jprogramId);
    auto& disabledAddons = Settings::values.disabled_addons[program_id];
    jobjectArray jdisabledAddonsArray =
        env->NewObjectArray(disabledAddons.size(), IDCache::GetStringClass(), ToJString(env, ""));
    for (size_t i = 0; i < disabledAddons.size(); ++i) {
        env->SetObjectArrayElement(jdisabledAddonsArray, i, ToJString(env, disabledAddons[i]));
    }
    return jdisabledAddonsArray;
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setDisabledAddons(JNIEnv* env, jobject obj,
                                                                  jstring jprogramId,
                                                                  jobjectArray jdisabledAddons) {
    auto program_id = EmulationSession::GetProgramId(env, jprogramId);
    Settings::values.disabled_addons[program_id].clear();
    std::vector<std::string> disabled_addons;
    const int size = env->GetArrayLength(jdisabledAddons);
    for (int i = 0; i < size; ++i) {
        auto jaddon = static_cast<jstring>(env->GetObjectArrayElement(jdisabledAddons, i));
        disabled_addons.push_back(GetJString(env, jaddon));
    }
    Settings::values.disabled_addons[program_id] = disabled_addons;
}

jobjectArray Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getOverlayControlData(JNIEnv* env,
                                                                              jobject obj) {
    jobjectArray joverlayControlDataArray =
        env->NewObjectArray(AndroidSettings::values.overlay_control_data.size(),
                            IDCache::GetOverlayControlDataClass(), nullptr);
    for (size_t i = 0; i < AndroidSettings::values.overlay_control_data.size(); ++i) {
        const auto& control_data = AndroidSettings::values.overlay_control_data[i];
        jobject jlandscapePosition =
            env->NewObject(IDCache::GetPairClass(), IDCache::GetPairConstructor(),
                           ToJDouble(env, control_data.landscape_position.first),
                           ToJDouble(env, control_data.landscape_position.second));
        jobject jportraitPosition =
            env->NewObject(IDCache::GetPairClass(), IDCache::GetPairConstructor(),
                           ToJDouble(env, control_data.portrait_position.first),
                           ToJDouble(env, control_data.portrait_position.second));
        jobject jfoldablePosition =
            env->NewObject(IDCache::GetPairClass(), IDCache::GetPairConstructor(),
                           ToJDouble(env, control_data.foldable_position.first),
                           ToJDouble(env, control_data.foldable_position.second));

        jobject jcontrolData = env->NewObject(
            IDCache::GetOverlayControlDataClass(), IDCache::GetOverlayControlDataConstructor(),
            ToJString(env, control_data.id), control_data.enabled, jlandscapePosition,
            jportraitPosition, jfoldablePosition);
        env->SetObjectArrayElement(joverlayControlDataArray, i, jcontrolData);
    }
    return joverlayControlDataArray;
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setOverlayControlData(
    JNIEnv* env, jobject obj, jobjectArray joverlayControlDataArray) {
    AndroidSettings::values.overlay_control_data.clear();
    int size = env->GetArrayLength(joverlayControlDataArray);

    if (size == 0) {
        return;
    }

    for (int i = 0; i < size; ++i) {
        jobject joverlayControlData = env->GetObjectArrayElement(joverlayControlDataArray, i);
        jstring jidString = static_cast<jstring>(
            env->GetObjectField(joverlayControlData, IDCache::GetOverlayControlDataIdField()));
        bool enabled = static_cast<bool>(env->GetBooleanField(
            joverlayControlData, IDCache::GetOverlayControlDataEnabledField()));

        jobject jlandscapePosition = env->GetObjectField(
            joverlayControlData, IDCache::GetOverlayControlDataLandscapePositionField());
        std::pair<double, double> landscape_position = std::make_pair(
            GetJDouble(env, env->GetObjectField(jlandscapePosition, IDCache::GetPairFirstField())),
            GetJDouble(env,
                       env->GetObjectField(jlandscapePosition, IDCache::GetPairSecondField())));

        jobject jportraitPosition = env->GetObjectField(
            joverlayControlData, IDCache::GetOverlayControlDataPortraitPositionField());
        std::pair<double, double> portrait_position = std::make_pair(
            GetJDouble(env, env->GetObjectField(jportraitPosition, IDCache::GetPairFirstField())),
            GetJDouble(env, env->GetObjectField(jportraitPosition, IDCache::GetPairSecondField())));

        jobject jfoldablePosition = env->GetObjectField(
            joverlayControlData, IDCache::GetOverlayControlDataFoldablePositionField());
        std::pair<double, double> foldable_position = std::make_pair(
            GetJDouble(env, env->GetObjectField(jfoldablePosition, IDCache::GetPairFirstField())),
            GetJDouble(env, env->GetObjectField(jfoldablePosition, IDCache::GetPairSecondField())));

        AndroidSettings::values.overlay_control_data.push_back(AndroidSettings::OverlayControlData{
            GetJString(env, jidString), enabled, landscape_position, portrait_position,
            foldable_position});
    }
}

} // extern "C"
