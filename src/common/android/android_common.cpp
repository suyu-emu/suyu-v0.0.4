// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// SPDX-FileCopyrightText: 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "android_common.h"

#include <string>
#include <string_view>

#include <jni.h>

#include "common/android/id_cache.h"
#include "common/string_util.h"

namespace Common::Android {

std::string GetJString(JNIEnv* env, jstring jstr) {
    if (!jstr) {
        return {};
    }

    const jchar* jchars = env->GetStringChars(jstr, nullptr);
    const jsize length = env->GetStringLength(jstr);
    const std::u16string_view string_view(reinterpret_cast<const char16_t*>(jchars),
                                          static_cast<u32>(length));
    const std::string converted_string = Common::UTF16ToUTF8(string_view);
    env->ReleaseStringChars(jstr, jchars);

    return converted_string;
}

jstring ToJString(JNIEnv* env, std::string_view str) {
    const std::u16string converted_string = Common::UTF8ToUTF16(str);
    return env->NewString(reinterpret_cast<const jchar*>(converted_string.data()),
                          static_cast<jint>(converted_string.size()));
}

jobjectArray ToJStringArray(JNIEnv* env, const std::vector<std::string>& strs) {
    jobjectArray array =
            env->NewObjectArray(static_cast<jsize>(strs.size()), env->FindClass("java/lang/String"), env->NewStringUTF(""));
    for (std::size_t i = 0; i < strs.size(); ++i) {
        env->SetObjectArrayElement(array, static_cast<jsize>(i), ToJString(env, strs[i]));
    }
    return array;
}

jstring ToJString(JNIEnv* env, std::u16string_view str) {
    return ToJString(env, Common::UTF16ToUTF8(str));
}

double GetJDouble(JNIEnv* env, jobject jdouble) {
    return env->GetDoubleField(jdouble, GetDoubleValueField());
}

jobject ToJDouble(JNIEnv* env, double value) {
    return env->NewObject(GetDoubleClass(), GetDoubleConstructor(), value);
}

s32 GetJInteger(JNIEnv* env, jobject jinteger) {
    return env->GetIntField(jinteger, GetIntegerValueField());
}

jobject ToJInteger(JNIEnv* env, s32 value) {
    return env->NewObject(GetIntegerClass(), GetIntegerConstructor(), value);
}

bool GetJBoolean(JNIEnv* env, jobject jboolean) {
    return env->GetBooleanField(jboolean, GetBooleanValueField());
}

jobject ToJBoolean(JNIEnv* env, bool value) {
    return env->NewObject(GetBooleanClass(), GetBooleanConstructor(), value);
}

} // namespace Common::Android
