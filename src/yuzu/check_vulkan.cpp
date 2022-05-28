#include "video_core/vulkan_common/vulkan_wrapper.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "video_core/vulkan_common/vulkan_instance.h"
#include "video_core/vulkan_common/vulkan_library.h"
#include "yuzu/check_vulkan.h"
#include "yuzu/uisettings.h"

constexpr char TEMP_FILE_NAME[] = "vulkan_check";

bool CheckVulkan() {
    if (UISettings::values.has_broken_vulkan) {
        return true;
    }

    LOG_DEBUG(Frontend, "Checking presence of Vulkan");

    const auto fs_config_loc = Common::FS::GetYuzuPath(Common::FS::YuzuPath::ConfigDir);
    const auto temp_file_loc = fs_config_loc / TEMP_FILE_NAME;

    if (std::filesystem::exists(temp_file_loc)) {
        LOG_WARNING(Frontend, "Detected recovery from previous failed Vulkan initialization");

        UISettings::values.has_broken_vulkan = true;
        std::filesystem::remove(temp_file_loc);
        return false;
    }

    std::ofstream temp_file_handle(temp_file_loc);
    temp_file_handle.close();

    try {
        Vulkan::vk::InstanceDispatch dld;
        const Common::DynamicLibrary library = Vulkan::OpenLibrary();
        const Vulkan::vk::Instance instance =
            Vulkan::CreateInstance(library, dld, VK_API_VERSION_1_0);

    } catch (const Vulkan::vk::Exception& exception) {
        LOG_ERROR(Frontend, "Failed to initialize Vulkan: {}", exception.what());
        UISettings::values.has_broken_vulkan = true;
        return false;
    }

    std::filesystem::remove(temp_file_loc);
    return true;
}
