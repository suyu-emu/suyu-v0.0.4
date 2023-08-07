// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include "common/dynamic_library.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "video_core/vulkan_common/vulkan_library.h"

namespace Vulkan {

std::shared_ptr<Common::DynamicLibrary> OpenLibrary(
    [[maybe_unused]] Core::Frontend::GraphicsContext* context) {
    LOG_DEBUG(Render_Vulkan, "Looking for a Vulkan library");
#if defined(ANDROID) && defined(ARCHITECTURE_arm64)
    // Android manages its Vulkan driver from the frontend.
    return context->GetDriverLibrary();
#else
    auto library = std::make_shared<Common::DynamicLibrary>();
#ifdef __APPLE__
    // Check if a path to a specific Vulkan library has been specified.
    char* const libvulkan_env = std::getenv("LIBVULKAN_PATH");
    if (!libvulkan_env || !library->Open(libvulkan_env)) {
        // Use the libvulkan.dylib from the application bundle.
        const auto filename =
            Common::FS::GetBundleDirectory() / "Contents/Frameworks/libvulkan.dylib";
        void(library->Open(Common::FS::PathToUTF8String(filename).c_str()));
    }
#else
    std::string filename = Common::DynamicLibrary::GetVersionedFilename("vulkan", 1);
    LOG_DEBUG(Render_Vulkan, "Trying Vulkan library: {}", filename);
    if (!library->Open(filename.c_str())) {
        // Android devices may not have libvulkan.so.1, only libvulkan.so.
        filename = Common::DynamicLibrary::GetVersionedFilename("vulkan");
        LOG_DEBUG(Render_Vulkan, "Trying Vulkan library (second attempt): {}", filename);
        void(library->Open(filename.c_str()));
    }
#endif
    return library;
#endif
}

} // namespace Vulkan
