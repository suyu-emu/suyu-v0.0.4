// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef QT_COMMON_H
#define QT_COMMON_H

#include <memory>
#include <QWindow>
#include <core/frontend/emu_window.h>
#include "core/core.h"
#include "core/file_sys/registered_cache.h"

#include <core/file_sys/vfs/vfs_real.h>

namespace QtCommon {

extern QWidget* rootObject;

extern std::unique_ptr<Core::System> system;
extern std::shared_ptr<FileSys::RealVfsFilesystem> vfs;
extern std::unique_ptr<FileSys::ManualContentProvider> provider;

typedef std::function<bool(std::size_t, std::size_t)> QtProgressCallback;

Core::Frontend::WindowSystemType GetWindowSystemType();

Core::Frontend::EmuWindow::WindowSystemInfo GetWindowSystemInfo(QWindow* window);

void Init(QWidget* root);

const QString tr(const char* str);
const QString tr(const std::string& str);

std::filesystem::path GetEdenCommand();
} // namespace QtCommon
#endif
