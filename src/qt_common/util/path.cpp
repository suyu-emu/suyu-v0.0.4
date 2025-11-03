// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_common/util/path.h"
#include <QDesktopServices>
#include <QString>
#include <QUrl>
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "qt_common/abstract/frontend.h"
#include <fmt/format.h>

namespace QtCommon::Path {

bool OpenShaderCache(u64 program_id, QObject *parent)
{
    const auto shader_cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir);
    const auto shader_cache_folder_path{shader_cache_dir / fmt::format("{:016x}", program_id)};
    if (!Common::FS::CreateDirs(shader_cache_folder_path)) {
        QtCommon::Frontend::ShowMessage(QMessageBox::Warning,
                                        tr("Error Opening Shader Cache"),
                                        tr("Failed to create or open shader cache for this title, "
                                        "ensure your app data directory has write permissions."),
                                        QMessageBox::Ok,
                                        parent);
    }

    const auto shader_path_string{Common::FS::PathToUTF8String(shader_cache_folder_path)};
    const auto qt_shader_cache_path = QString::fromStdString(shader_path_string);
    return QDesktopServices::openUrl(QUrl::fromLocalFile(qt_shader_cache_path));
}

}
