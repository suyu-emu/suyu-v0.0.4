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

} // namespace IDCache
