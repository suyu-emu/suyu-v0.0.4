// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/fs/fs.h"
#include "qt_common.h"

#include <QGuiApplication>
#include <QStringLiteral>
#include "common/logging.h"
#include "core/frontend/emu_window.h"

#include <QFile>

#include <JlCompress.h>

#if !defined(WIN32) && !defined(__APPLE__)
#include <qpa/qplatformnativeinterface.h>
#elif defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace QtCommon {

QWidget* rootObject = nullptr;

std::unique_ptr<Core::System> system = nullptr;
std::shared_ptr<FileSys::RealVfsFilesystem> vfs = nullptr;
std::unique_ptr<FileSys::ManualContentProvider> provider = nullptr;

Core::Frontend::WindowSystemType GetWindowSystemType() {
    // Determine WSI type based on Qt platform.
    QString platform_name = QGuiApplication::platformName();
    if (platform_name == QStringLiteral("windows"))
        return Core::Frontend::WindowSystemType::Windows;
    else if (platform_name == QStringLiteral("xcb"))
        return Core::Frontend::WindowSystemType::X11;
    else if (platform_name == QStringLiteral("wayland"))
        return Core::Frontend::WindowSystemType::Wayland;
    else if (platform_name == QStringLiteral("wayland-egl"))
        return Core::Frontend::WindowSystemType::Wayland;
    else if (platform_name == QStringLiteral("cocoa"))
        return Core::Frontend::WindowSystemType::Cocoa;
    else if (platform_name == QStringLiteral("android"))
        return Core::Frontend::WindowSystemType::Android;
    else if (platform_name == QStringLiteral("haiku"))
        return Core::Frontend::WindowSystemType::Xcb;

    LOG_CRITICAL(Frontend, "Unknown Qt platform {}!", platform_name.toStdString());
    return Core::Frontend::WindowSystemType::Windows;
} // namespace Core::Frontend::WindowSystemType

Core::Frontend::EmuWindow::WindowSystemInfo GetWindowSystemInfo(QWindow* window) {
    Core::Frontend::EmuWindow::WindowSystemInfo wsi;
    wsi.type = GetWindowSystemType();

#if defined(WIN32)
    // Our Win32 Qt external doesn't have the private API.
    wsi.render_surface = reinterpret_cast<void*>(window->winId());
#elif defined(__APPLE__)
    id layer = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(
        reinterpret_cast<id>(window->winId()), sel_registerName("layer"));

    // In Qt 6, the layer of the NSView might be a QContainerLayer.
    // MoltenVK needs a CAMetalLayer. We search for it in sublayers.
    Class metal_layer_class = objc_getClass("CAMetalLayer");
    id metal_layer = nullptr;

    if (layer) {
        if (reinterpret_cast<bool (*)(id, SEL, Class)>(objc_msgSend)(
                layer, sel_registerName("isKindOfClass:"), metal_layer_class)) {
            metal_layer = layer;
        } else {
            id sublayers = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(
                layer, sel_registerName("sublayers"));
            if (sublayers) {
                unsigned long count = reinterpret_cast<unsigned long (*)(id, SEL)>(objc_msgSend)(
                    sublayers, sel_registerName("count"));
                for (unsigned long i = 0; i < count; ++i) {
                    id sublayer = reinterpret_cast<id (*)(id, SEL, unsigned long)>(objc_msgSend)(
                        sublayers, sel_registerName("objectAtIndex:"), i);
                    if (reinterpret_cast<bool (*)(id, SEL, Class)>(objc_msgSend)(
                            sublayer, sel_registerName("isKindOfClass:"), metal_layer_class)) {
                        metal_layer = sublayer;
                        break;
                    }
                }
            }
        }
    }
    wsi.render_surface = reinterpret_cast<void*>(metal_layer ? metal_layer : layer);
#else
    QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
    wsi.display_connection = pni->nativeResourceForWindow("display", window);
    if (wsi.type == Core::Frontend::WindowSystemType::Wayland)
        wsi.render_surface = window ? pni->nativeResourceForWindow("surface", window) : nullptr;
    else
        wsi.render_surface = window ? reinterpret_cast<void*>(window->winId()) : nullptr;
#endif
    wsi.render_surface_scale = window ? static_cast<float>(window->devicePixelRatio()) : 1.0f;

    return wsi;
}

const QString tr(const char* str) {
    return QGuiApplication::tr(str);
}

const QString tr(const std::string& str) {
    return QGuiApplication::tr(str.c_str());
}

void Init(QWidget* root) {
    system = std::make_unique<Core::System>();
    rootObject = root;
    vfs = std::make_unique<FileSys::RealVfsFilesystem>();
    provider = std::make_unique<FileSys::ManualContentProvider>();
}

std::filesystem::path GetEdenCommand() {
    std::filesystem::path command;

    // TODO: flatpak?
    QString appimage = QString::fromLocal8Bit(getenv("APPIMAGE"));
    if (!appimage.isEmpty()) {
        command = std::filesystem::path{appimage.toStdString()};
    } else {
        const QStringList args = QGuiApplication::arguments();
        command = args[0].toStdString();
    }

    // If relative path, make it an absolute path
    if (command.c_str()[0] == '.') {
        command = Common::FS::GetCurrentDir() / command;
    }

    return command;
}

} // namespace QtCommon
