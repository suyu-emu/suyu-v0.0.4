// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/settings.h"
#include "input_common/touch_from_button.h"

namespace InputCommon {

class TouchFromButtonDevice final : public Input::TouchDevice {
public:
    TouchFromButtonDevice() {
        for (const auto& config_entry :
             Settings::values.touch_from_button_maps[Settings::values.touch_from_button_map_index]
                 .buttons) {
            const Common::ParamPackage package{config_entry};
            map.emplace_back(
                Input::CreateDevice<Input::ButtonDevice>(config_entry),
                std::clamp(package.Get("x", 0), 0, static_cast<int>(Layout::ScreenUndocked::Width)),
                std::clamp(package.Get("y", 0), 0,
                           static_cast<int>(Layout::ScreenUndocked::Height)));
        }
    }

    std::tuple<float, float, bool> GetStatus() const override {
        for (const auto& m : map) {
            const bool state = std::get<0>(m)->GetStatus();
            if (state) {
                const float x = static_cast<float>(std::get<1>(m)) /
                                static_cast<int>(Layout::ScreenUndocked::Width);
                const float y = static_cast<float>(std::get<2>(m)) /
                                static_cast<int>(Layout::ScreenUndocked::Height);
                return std::make_tuple(x, y, true);
            }
        }
        return std::make_tuple(0.0f, 0.0f, false);
    }

private:
    std::vector<std::tuple<std::unique_ptr<Input::ButtonDevice>, int, int>> map; // button, x, y
};

std::unique_ptr<Input::TouchDevice> TouchFromButtonFactory::Create(
    const Common::ParamPackage& params) {

    return std::make_unique<TouchFromButtonDevice>();
}

} // namespace InputCommon
