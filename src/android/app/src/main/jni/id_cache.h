#pragma once

#include <jni.h>

namespace IDCache {

JNIEnv* GetEnvForThread();
jclass GetNativeLibraryClass();
jmethodID GetExitEmulationActivity();

} // namespace IDCache
