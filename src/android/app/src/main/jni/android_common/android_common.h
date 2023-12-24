// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <jni.h>

std::string GetJString(JNIEnv* env, jstring jstr);
jstring ToJString(JNIEnv* env, std::string_view str);
jstring ToJString(JNIEnv* env, std::u16string_view str);

double GetJDouble(JNIEnv* env, jobject jdouble);
jobject ToJDouble(JNIEnv* env, double value);
