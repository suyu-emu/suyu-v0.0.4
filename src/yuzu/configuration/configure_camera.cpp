// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Text : Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <memory>
#include <QtCore>
#if YUZU_USE_QT_MULTIMEDIA
#include <QCamera>
#include <QImageCapture>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#endif
#include <QStandardItemModel>
#include <QTimer>

#include "common/settings.h"
#include "input_common/drivers/camera.h"
#include "input_common/main.h"
#include "ui_configure_camera.h"
#include "yuzu/configuration/configure_camera.h"

ConfigureCamera::ConfigureCamera(QWidget* parent, InputCommon::InputSubsystem* input_subsystem_)
    : QDialog(parent), input_subsystem{input_subsystem_},
      ui(std::make_unique<Ui::ConfigureCamera>()) {
    ui->setupUi(this);

    connect(ui->restore_defaults_button, &QPushButton::clicked, this,
            &ConfigureCamera::RestoreDefaults);
    connect(ui->preview_button, &QPushButton::clicked, this, &ConfigureCamera::PreviewCamera);

    auto blank_image = QImage(320, 240, QImage::Format::Format_RGB32);
    blank_image.fill(Qt::black);
    DisplayCapturedFrame(0, blank_image);

    LoadConfiguration();
    resize(0, 0);
}

ConfigureCamera::~ConfigureCamera() = default;

void ConfigureCamera::PreviewCamera() {
#if YUZU_USE_QT_MULTIMEDIA
    const auto index = ui->ir_sensor_combo_box->currentIndex();
    bool camera_found = false;
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    for (const QCameraDevice& cameraDevice : cameras) {
        if (input_devices[index] == cameraDevice.id().toStdString() ||
            input_devices[index] == "auto") {
            LOG_INFO(Frontend, "Selected Camera {} {}", cameraDevice.description().toStdString(),
                     cameraDevice.id().toStdString());
            if (cameraDevice.videoFormats().isEmpty()) {
                LOG_ERROR(Frontend, "Camera doesn't provide any video formats.");
                continue;
            }
            camera = std::make_unique<QCamera>(cameraDevice);
            camera_found = true;
            break;
        }
    }

    // Clear previous frame
    auto blank_image = QImage(320, 240, QImage::Format::Format_RGB32);
    blank_image.fill(Qt::black);
    DisplayCapturedFrame(0, blank_image);

    if (!camera_found) {
        return;
    }

    capture_session = std::make_unique<QMediaCaptureSession>();
    camera_capture = std::make_unique<QImageCapture>();
    capture_session->setCamera(camera.get());
    capture_session->setImageCapture(camera_capture.get());
    connect(camera_capture.get(), &QImageCapture::imageCaptured, this,
            &ConfigureCamera::DisplayCapturedFrame);
    camera->start();

    pending_snapshots = 0;
    is_virtual_camera = false;

    camera_timer = std::make_unique<QTimer>();
    connect(camera_timer.get(), &QTimer::timeout, [this] {
        // If the camera doesn't capture, test for virtual cameras
        if (pending_snapshots > 5) {
            is_virtual_camera = true;
        }
        // Virtual cameras like obs need to reset the camera every capture
        if (is_virtual_camera) {
            camera->stop();
            camera->start();
        }
        pending_snapshots++;
        camera_capture->capture();
    });

    camera_timer->start(250);
#endif
}

void ConfigureCamera::DisplayCapturedFrame(int requestId, const QImage& img) {
    LOG_INFO(Frontend, "ImageCaptured {} {}", img.width(), img.height());
    const auto converted = img.scaled(320, 240, Qt::AspectRatioMode::IgnoreAspectRatio,
                                      Qt::TransformationMode::SmoothTransformation);
    ui->preview_box->setPixmap(QPixmap::fromImage(converted));
    pending_snapshots = 0;
}

void ConfigureCamera::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QDialog::changeEvent(event);
}

void ConfigureCamera::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureCamera::ApplyConfiguration() {
    std::string current_device = input_devices[ui->ir_sensor_combo_box->currentIndex()];
#ifdef _WIN32
    // for whatever reason replacing with / isn't enough so we use | for saving
    std::replace(current_device.begin(), current_device.end(), '\\', '|');
#endif
    Settings::values.ir_sensor_device.SetValue(current_device);
}

void ConfigureCamera::LoadConfiguration() {
    input_devices.clear();
    ui->ir_sensor_combo_box->clear();
    input_devices.push_back("auto");
    ui->ir_sensor_combo_box->addItem(tr("Auto"));
#if YUZU_USE_QT_MULTIMEDIA
    const auto cameras = QMediaDevices::videoInputs();
    for (const QCameraDevice& cameraDevice : cameras) {
        input_devices.push_back(cameraDevice.id().toStdString());
        ui->ir_sensor_combo_box->addItem(cameraDevice.description());
    }
#endif

    std::string current_device = Settings::values.ir_sensor_device.GetValue();
#ifdef _WIN32
    std::replace(current_device.begin(), current_device.end(), '|', '\\');
#endif

    const auto devices_it = std::find_if(
        input_devices.begin(), input_devices.end(),
        [current_device](const std::string& device) { return device == current_device; });
    const int device_index =
        devices_it != input_devices.end()
            ? static_cast<int>(std::distance(input_devices.begin(), devices_it))
            : 0;
    ui->ir_sensor_combo_box->setCurrentIndex(device_index);
}

void ConfigureCamera::RestoreDefaults() {
    ui->ir_sensor_combo_box->setCurrentIndex(0);
}
