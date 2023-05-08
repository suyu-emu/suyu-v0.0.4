// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <QColor>
#include <QString>
#include <QWidget>
#include <qobjectdefs.h>
#include <vulkan/vulkan_core.h>
#include "common/common_types.h"
#include "vk_device_info.h"
#include "yuzu/configuration/configuration_shared.h"

class QEvent;
class QObject;
class QComboBox;

namespace Settings {
enum class NvdecEmulation : u32;
enum class RendererBackend : u32;
enum class ShaderBackend : u32;
} // namespace Settings

namespace Core {
class System;
}

namespace Ui {
class ConfigureGraphics;
}

class ConfigureGraphics : public ConfigurationShared::Tab {
public:
    explicit ConfigureGraphics(const Core::System& system_,
                               std::vector<VkDeviceInfo::Record>& records,
                               const std::function<void()>& expose_compute_option_,
                               std::shared_ptr<std::forward_list<ConfigurationShared::Tab*>> group,
                               const ConfigurationShared::TranslationMap& translations_,
                               QWidget* parent = nullptr);
    ~ConfigureGraphics() override;

    void ApplyConfiguration() override;
    void SetConfiguration() override;

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void PopulateVSyncModeSelection();
    void UpdateBackgroundColorButton(QColor color);
    void UpdateAPILayout();
    void UpdateDeviceSelection(int device);
    void UpdateShaderBackendSelection(int backend);

    void RetrieveVulkanDevices();

    void SetFSRIndicatorText(int percentage);
    /* Turns a Vulkan present mode into a textual string for a UI
     * (and eventually for a human to read) */
    const QString TranslateVSyncMode(VkPresentModeKHR mode,
                                     Settings::RendererBackend backend) const;

    Settings::RendererBackend GetCurrentGraphicsBackend() const;

    std::unique_ptr<Ui::ConfigureGraphics> ui;
    QColor bg_color;

    std::list<ConfigurationShared::CheckState> trackers{};
    std::forward_list<std::function<void(bool)>> apply_funcs{};

    std::vector<VkDeviceInfo::Record>& records;
    std::vector<QString> vulkan_devices;
    std::vector<std::vector<VkPresentModeKHR>> device_present_modes;
    std::vector<VkPresentModeKHR>
        vsync_mode_combobox_enum_map; //< Keeps track of which present mode corresponds to which
                                      // selection in the combobox
    u32 vulkan_device{};
    Settings::ShaderBackend shader_backend{};
    const std::function<void()>& expose_compute_option;

    const Core::System& system;
    const ConfigurationShared::TranslationMap& translations;

    QPushButton* api_restore_global_button;
    QComboBox* vulkan_device_combobox;
    QComboBox* api_combobox;
    QComboBox* shader_backend_combobox;
    QComboBox* vsync_mode_combobox;
    QWidget* vulkan_device_widget;
    QWidget* api_widget;
    QWidget* shader_backend_widget;
};
