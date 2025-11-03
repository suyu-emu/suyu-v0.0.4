// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <renderdoc_app.h>

#include "common/assert.h"
#include "common/dynamic_library.h"
#include "core/tools/renderdoc.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Tools {

RenderdocAPI::RenderdocAPI() {
#ifdef WIN32
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
        void* proc = reinterpret_cast<void*>(GetProcAddress(mod, "RENDERDOC_GetAPI"));
        if (proc) {
            const auto RENDERDOC_GetAPI = reinterpret_cast<pRENDERDOC_GetAPI>(proc);
            const s32 ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);
            ASSERT(ret == 1);
        }
    }
#elif defined(__HAIKU__)
    // no rtld on haiku
#else
#ifdef ANDROID
    static constexpr const char RENDERDOC_LIB[] = "libVkLayer_GLES_RenderDoc.so";
#else
    static constexpr const char RENDERDOC_LIB[] = "librenderdoc.so";
#endif
    if (void* mod = dlopen(RENDERDOC_LIB, RTLD_NOW | RTLD_NOLOAD)) {
        const auto RENDERDOC_GetAPI =
            reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(mod, "RENDERDOC_GetAPI"));
        const s32 ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);
        ASSERT(ret == 1);
    }
#endif
}

RenderdocAPI::~RenderdocAPI() = default;

void RenderdocAPI::ToggleCapture() {
    if (!rdoc_api) [[unlikely]] {
        return;
    }
    if (!is_capturing) {
        rdoc_api->StartFrameCapture(NULL, NULL);
    } else {
        rdoc_api->EndFrameCapture(NULL, NULL);
    }
    is_capturing = !is_capturing;
}

} // namespace Tools
