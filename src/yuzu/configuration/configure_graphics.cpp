// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QColorDialog>
#include <QComboBox>
#ifdef HAS_VULKAN
#include <QVulkanInstance>
#endif

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_graphics.h"
#include "yuzu/configuration/configure_graphics.h"

namespace {
enum class Resolution : int {
    Auto,
    Scale1x,
    Scale2x,
    Scale3x,
    Scale4x,
};

float ToResolutionFactor(Resolution option) {
    switch (option) {
    case Resolution::Auto:
        return 0.f;
    case Resolution::Scale1x:
        return 1.f;
    case Resolution::Scale2x:
        return 2.f;
    case Resolution::Scale3x:
        return 3.f;
    case Resolution::Scale4x:
        return 4.f;
    }
    return 0.f;
}

Resolution FromResolutionFactor(float factor) {
    if (factor == 0.f) {
        return Resolution::Auto;
    } else if (factor == 1.f) {
        return Resolution::Scale1x;
    } else if (factor == 2.f) {
        return Resolution::Scale2x;
    } else if (factor == 3.f) {
        return Resolution::Scale3x;
    } else if (factor == 4.f) {
        return Resolution::Scale4x;
    }
    return Resolution::Auto;
}
} // Anonymous namespace

ConfigureGraphics::ConfigureGraphics(QWidget* parent)
    : QWidget(parent), ui(new Ui::ConfigureGraphics) {
    vulkan_device = Settings::values.vulkan_device;
    RetrieveVulkanDevices();

    ui->setupUi(this);

    SetConfiguration();

    connect(ui->api, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this] { UpdateDeviceComboBox(); });
    connect(ui->device, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
            [this](int device) { UpdateDeviceSelection(device); });

    connect(ui->bg_button, &QPushButton::clicked, this, [this] {
        const QColor new_bg_color = QColorDialog::getColor(bg_color);
        if (!new_bg_color.isValid()) {
            return;
        }
        UpdateBackgroundColorButton(new_bg_color);
    });
}

void ConfigureGraphics::UpdateDeviceSelection(int device) {
    if (device == -1) {
        return;
    }
    if (GetCurrentGraphicsBackend() == Settings::RendererBackend::Vulkan) {
        vulkan_device = device;
    }
}

ConfigureGraphics::~ConfigureGraphics() = default;

void ConfigureGraphics::SetConfiguration() {
    const bool runtime_lock = !Core::System::GetInstance().IsPoweredOn();

    ui->api->setEnabled(runtime_lock);
    ui->api->setCurrentIndex(static_cast<int>(Settings::values.renderer_backend));
    ui->resolution_factor_combobox->setCurrentIndex(
        static_cast<int>(FromResolutionFactor(Settings::values.resolution_factor)));
    ui->use_disk_shader_cache->setEnabled(runtime_lock);
    ui->use_disk_shader_cache->setChecked(Settings::values.use_disk_shader_cache);
    ui->use_accurate_gpu_emulation->setChecked(Settings::values.use_accurate_gpu_emulation);
    ui->use_asynchronous_gpu_emulation->setEnabled(runtime_lock);
    ui->use_asynchronous_gpu_emulation->setChecked(Settings::values.use_asynchronous_gpu_emulation);
    ui->force_30fps_mode->setEnabled(runtime_lock);
    ui->force_30fps_mode->setChecked(Settings::values.force_30fps_mode);
    UpdateBackgroundColorButton(QColor::fromRgbF(Settings::values.bg_red, Settings::values.bg_green,
                                                 Settings::values.bg_blue));
    UpdateDeviceComboBox();
}

void ConfigureGraphics::ApplyConfiguration() {
    Settings::values.renderer_backend = GetCurrentGraphicsBackend();
    Settings::values.vulkan_device = vulkan_device;
    Settings::values.resolution_factor =
        ToResolutionFactor(static_cast<Resolution>(ui->resolution_factor_combobox->currentIndex()));
    Settings::values.use_disk_shader_cache = ui->use_disk_shader_cache->isChecked();
    Settings::values.use_accurate_gpu_emulation = ui->use_accurate_gpu_emulation->isChecked();
    Settings::values.use_asynchronous_gpu_emulation =
        ui->use_asynchronous_gpu_emulation->isChecked();
    Settings::values.force_30fps_mode = ui->force_30fps_mode->isChecked();
    Settings::values.bg_red = static_cast<float>(bg_color.redF());
    Settings::values.bg_green = static_cast<float>(bg_color.greenF());
    Settings::values.bg_blue = static_cast<float>(bg_color.blueF());
}

void ConfigureGraphics::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureGraphics::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureGraphics::UpdateBackgroundColorButton(QColor color) {
    bg_color = color;

    QPixmap pixmap(ui->bg_button->size());
    pixmap.fill(bg_color);

    const QIcon color_icon(pixmap);
    ui->bg_button->setIcon(color_icon);
}

void ConfigureGraphics::UpdateDeviceComboBox() {
    ui->device->clear();

    bool enabled = false;
    switch (GetCurrentGraphicsBackend()) {
    case Settings::RendererBackend::OpenGL:
        ui->device->addItem(tr("OpenGL Graphics Device"));
        enabled = false;
        break;
    case Settings::RendererBackend::Vulkan:
        for (const auto device : vulkan_devices) {
            ui->device->addItem(device);
        }
        ui->device->setCurrentIndex(vulkan_device);
        enabled = !vulkan_devices.empty();
        break;
    }
    ui->device->setEnabled(enabled && !Core::System::GetInstance().IsPoweredOn());
}

void ConfigureGraphics::RetrieveVulkanDevices() {
#ifdef HAS_VULKAN
    QVulkanInstance instance;
    instance.setApiVersion(QVersionNumber(1, 1, 0));
    if (!instance.create()) {
        LOG_INFO(Frontend, "Vulkan 1.1 not available");
        return;
    }
    const auto vkEnumeratePhysicalDevices{reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        instance.getInstanceProcAddr("vkEnumeratePhysicalDevices"))};
    if (vkEnumeratePhysicalDevices == nullptr) {
        LOG_INFO(Frontend, "Failed to get pointer to vkEnumeratePhysicalDevices");
        return;
    }
    u32 physical_device_count;
    if (vkEnumeratePhysicalDevices(instance.vkInstance(), &physical_device_count, nullptr) !=
        VK_SUCCESS) {
        LOG_INFO(Frontend, "Failed to get physical devices count");
        return;
    }
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    if (vkEnumeratePhysicalDevices(instance.vkInstance(), &physical_device_count,
                                   physical_devices.data()) != VK_SUCCESS) {
        LOG_INFO(Frontend, "Failed to get physical devices");
        return;
    }

    const auto vkGetPhysicalDeviceProperties{reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        instance.getInstanceProcAddr("vkGetPhysicalDeviceProperties"))};
    if (vkGetPhysicalDeviceProperties == nullptr) {
        LOG_INFO(Frontend, "Failed to get pointer to vkGetPhysicalDeviceProperties");
        return;
    }
    for (const auto physical_device : physical_devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physical_device, &properties);
        vulkan_devices.push_back(QString::fromUtf8(properties.deviceName));
    }
#endif
}

Settings::RendererBackend ConfigureGraphics::GetCurrentGraphicsBackend() const {
    return static_cast<Settings::RendererBackend>(ui->api->currentIndex());
}
