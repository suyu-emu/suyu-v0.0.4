// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include <jni.h>

#include "android_config.h"
#include "android_settings.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "frontend_common/config.h"
#include "jni/android_common/android_common.h"
#include "jni/id_cache.h"

std::unique_ptr<AndroidConfig> config;

template <typename T>
Settings::Setting<T>* getSetting(JNIEnv* env, jstring jkey) {
    auto key = GetJString(env, jkey);
    auto basicSetting = Settings::values.linkage.by_key[key];
    auto basicAndroidSetting = AndroidSettings::values.linkage.by_key[key];
    if (basicSetting != 0) {
        return static_cast<Settings::Setting<T>*>(basicSetting);
    }
    if (basicAndroidSetting != 0) {
        return static_cast<Settings::Setting<T>*>(basicAndroidSetting);
    }
    LOG_ERROR(Frontend, "[Android Native] Could not find setting - {}", key);
    return nullptr;
}

extern "C" {

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_initializeConfig(JNIEnv* env, jobject obj) {
    config = std::make_unique<AndroidConfig>();
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_unloadConfig(JNIEnv* env, jobject obj) {
    config.reset();
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_reloadSettings(JNIEnv* env, jobject obj) {
    config->AndroidConfig::ReloadAllValues();
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_saveSettings(JNIEnv* env, jobject obj) {
    config->AndroidConfig::SaveAllValues();
}

jboolean Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getBoolean(JNIEnv* env, jobject obj,
                                                               jstring jkey, jboolean getDefault) {
    auto setting = getSetting<bool>(env, jkey);
    if (setting == nullptr) {
        return false;
    }
    setting->SetGlobal(true);

    if (static_cast<bool>(getDefault)) {
        return setting->GetDefault();
    }

    return setting->GetValue();
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setBoolean(JNIEnv* env, jobject obj, jstring jkey,
                                                           jboolean value) {
    auto setting = getSetting<bool>(env, jkey);
    if (setting == nullptr) {
        return;
    }
    setting->SetGlobal(true);
    setting->SetValue(static_cast<bool>(value));
}

jbyte Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getByte(JNIEnv* env, jobject obj, jstring jkey,
                                                         jboolean getDefault) {
    auto setting = getSetting<u8>(env, jkey);
    if (setting == nullptr) {
        return -1;
    }
    setting->SetGlobal(true);

    if (static_cast<bool>(getDefault)) {
        return setting->GetDefault();
    }

    return setting->GetValue();
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setByte(JNIEnv* env, jobject obj, jstring jkey,
                                                        jbyte value) {
    auto setting = getSetting<u8>(env, jkey);
    if (setting == nullptr) {
        return;
    }
    setting->SetGlobal(true);
    setting->SetValue(value);
}

jshort Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getShort(JNIEnv* env, jobject obj, jstring jkey,
                                                           jboolean getDefault) {
    auto setting = getSetting<u16>(env, jkey);
    if (setting == nullptr) {
        return -1;
    }
    setting->SetGlobal(true);

    if (static_cast<bool>(getDefault)) {
        return setting->GetDefault();
    }

    return setting->GetValue();
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setShort(JNIEnv* env, jobject obj, jstring jkey,
                                                         jshort value) {
    auto setting = getSetting<u16>(env, jkey);
    if (setting == nullptr) {
        return;
    }
    setting->SetGlobal(true);
    setting->SetValue(value);
}

jint Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getInt(JNIEnv* env, jobject obj, jstring jkey,
                                                       jboolean getDefault) {
    auto setting = getSetting<int>(env, jkey);
    if (setting == nullptr) {
        return -1;
    }
    setting->SetGlobal(true);

    if (static_cast<bool>(getDefault)) {
        return setting->GetDefault();
    }

    return setting->GetValue();
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setInt(JNIEnv* env, jobject obj, jstring jkey,
                                                       jint value) {
    auto setting = getSetting<int>(env, jkey);
    if (setting == nullptr) {
        return;
    }
    setting->SetGlobal(true);
    setting->SetValue(value);
}

jfloat Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getFloat(JNIEnv* env, jobject obj, jstring jkey,
                                                           jboolean getDefault) {
    auto setting = getSetting<float>(env, jkey);
    if (setting == nullptr) {
        return -1;
    }
    setting->SetGlobal(true);

    if (static_cast<bool>(getDefault)) {
        return setting->GetDefault();
    }

    return setting->GetValue();
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setFloat(JNIEnv* env, jobject obj, jstring jkey,
                                                         jfloat value) {
    auto setting = getSetting<float>(env, jkey);
    if (setting == nullptr) {
        return;
    }
    setting->SetGlobal(true);
    setting->SetValue(value);
}

jlong Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getLong(JNIEnv* env, jobject obj, jstring jkey,
                                                         jboolean getDefault) {
    auto setting = getSetting<long>(env, jkey);
    if (setting == nullptr) {
        return -1;
    }
    setting->SetGlobal(true);

    if (static_cast<bool>(getDefault)) {
        return setting->GetDefault();
    }

    return setting->GetValue();
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setLong(JNIEnv* env, jobject obj, jstring jkey,
                                                        jlong value) {
    auto setting = getSetting<long>(env, jkey);
    if (setting == nullptr) {
        return;
    }
    setting->SetGlobal(true);
    setting->SetValue(value);
}

jstring Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getString(JNIEnv* env, jobject obj, jstring jkey,
                                                             jboolean getDefault) {
    auto setting = getSetting<std::string>(env, jkey);
    if (setting == nullptr) {
        return ToJString(env, "");
    }
    setting->SetGlobal(true);

    if (static_cast<bool>(getDefault)) {
        return ToJString(env, setting->GetDefault());
    }

    return ToJString(env, setting->GetValue());
}

void Java_org_yuzu_yuzu_1emu_utils_NativeConfig_setString(JNIEnv* env, jobject obj, jstring jkey,
                                                          jstring value) {
    auto setting = getSetting<std::string>(env, jkey);
    if (setting == nullptr) {
        return;
    }

    setting->SetGlobal(true);
    setting->SetValue(GetJString(env, value));
}

jboolean Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getIsRuntimeModifiable(JNIEnv* env, jobject obj,
                                                                           jstring jkey) {
    auto key = GetJString(env, jkey);
    auto setting = Settings::values.linkage.by_key[key];
    if (setting != 0) {
        return setting->RuntimeModfiable();
    }
    LOG_ERROR(Frontend, "[Android Native] Could not find setting - {}", key);
    return true;
}

jstring Java_org_yuzu_yuzu_1emu_utils_NativeConfig_getConfigHeader(JNIEnv* env, jobject obj,
                                                                   jint jcategory) {
    auto category = static_cast<Settings::Category>(jcategory);
    return ToJString(env, Settings::TranslateCategory(category));
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

} // extern "C"
