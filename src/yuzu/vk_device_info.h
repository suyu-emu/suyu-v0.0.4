#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "vulkan/vulkan_core.h"

namespace Settings {
enum class VSyncMode : u32;
}
// #include "common/settings.h"

namespace VkDeviceInfo {
// Short class to record Vulkan driver information for configuration purposes
class Record {
public:
    explicit Record(std::string_view name, const std::vector<VkPresentModeKHR>& vsync_modes,
                    bool is_intel_proprietary);
    ~Record();

    const std::string name;
    const std::vector<VkPresentModeKHR> vsync_support;
    const bool is_intel_proprietary;
};

void PopulateRecords(std::vector<Record>& records, QWindow* window);
} // namespace VkDeviceInfo