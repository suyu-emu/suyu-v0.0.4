#include "jni/id_cache.h"

static JavaVM* s_java_vm;
static jclass s_native_library_class;
static jmethodID s_exit_emulation_activity;

namespace IDCache {

JNIEnv* GetEnvForThread() {
    thread_local static struct OwnedEnv {
        OwnedEnv() {
            status = s_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
            if (status == JNI_EDETACHED)
                s_java_vm->AttachCurrentThread(&env, nullptr);
        }

        ~OwnedEnv() {
            if (status == JNI_EDETACHED)
                s_java_vm->DetachCurrentThread();
        }

        int status;
        JNIEnv* env = nullptr;
    } owned;
    return owned.env;
}

jclass GetNativeLibraryClass() {
    return s_native_library_class;
}

jmethodID GetExitEmulationActivity() {
    return s_exit_emulation_activity;
}

} // namespace IDCache
