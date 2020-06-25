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

#ifdef HAS_VULKAN
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#endif

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
    ui->aspect_ratio_combobox->setCurrentIndex(Settings::values.aspect_ratio);
    ui->use_disk_shader_cache->setEnabled(runtime_lock);
    ui->use_disk_shader_cache->setChecked(Settings::values.use_disk_shader_cache);
    ui->use_asynchronous_gpu_emulation->setEnabled(runtime_lock);
    ui->use_asynchronous_gpu_emulation->setChecked(Settings::values.use_asynchronous_gpu_emulation);
    UpdateBackgroundColorButton(QColor::fromRgbF(Settings::values.bg_red, Settings::values.bg_green,
                                                 Settings::values.bg_blue));
    UpdateDeviceComboBox();
}

void ConfigureGraphics::ApplyConfiguration() {
    Settings::values.renderer_backend = GetCurrentGraphicsBackend();
    Settings::values.vulkan_device = vulkan_device;
    Settings::values.aspect_ratio = ui->aspect_ratio_combobox->currentIndex();
    Settings::values.use_disk_shader_cache = ui->use_disk_shader_cache->isChecked();
    Settings::values.use_asynchronous_gpu_emulation =
        ui->use_asynchronous_gpu_emulation->isChecked();
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
    vulkan_devices.clear();
    for (auto& name : Vulkan::RendererVulkan::EnumerateDevices()) {
        vulkan_devices.push_back(QString::fromStdString(name));
    }
#endif
}

Settings::RendererBackend ConfigureGraphics::GetCurrentGraphicsBackend() const {
    return static_cast<Settings::RendererBackend>(ui->api->currentIndex());
}
