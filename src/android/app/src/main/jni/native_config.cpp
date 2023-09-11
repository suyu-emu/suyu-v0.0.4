// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include <jni.h>

#include "common/logging/log.h"
#include "common/settings.h"
#include "jni/android_common/android_common.h"
#include "jni/config.h"
#include "uisettings.h"

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

} // extern "C"
