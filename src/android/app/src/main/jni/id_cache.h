// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <jni.h>

#include "video_core/rasterizer_interface.h"

namespace IDCache {

JNIEnv* GetEnvForThread();
jclass GetNativeLibraryClass();
jclass GetDiskCacheProgressClass();
jclass GetDiskCacheLoadCallbackStageClass();
jmethodID GetExitEmulationActivity();
jmethodID GetDiskCacheLoadProgress();
jmethodID GetOnEmulationStarted();
jmethodID GetOnEmulationStopped();

} // namespace IDCache
