// SPDX-FileCopyrightText: Copyright 2026 suyu Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "qt_common/qt_common.h"
#include "qt_common/util/content.h"
#include "qt_common/util/fs.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QString>

#include <filesystem>

#include "frontend_common/data_manager.h"

namespace QtCommon {
std::unique_ptr<Core::System> system;
std::shared_ptr<FileSys::RealVfsFilesystem> vfs;
std::unique_ptr<FileSys::ManualContentProvider> provider;
} // namespace QtCommon

namespace QtCommon::FS {

void LinkRyujinx(std::filesystem::path& from, std::filesystem::path& to) {
    std::error_code ec;
    std::filesystem::create_directories(to, ec);
    std::filesystem::copy(from, to,
                          std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing,
                          ec);
}

} // namespace QtCommon::FS

namespace QtCommon::Content {

void ClearDataDir(FrontendCommon::DataManager::DataDir dir, const std::string& user_id) {
    FrontendCommon::DataManager::ClearDir(dir, user_id);
}

void ExportDataDir(FrontendCommon::DataManager::DataDir dir, const std::string& user_id,
                   const QString& name, std::function<void()> callback) {
    namespace fs = std::filesystem;
    const auto source_dir =
        QString::fromStdString(FrontendCommon::DataManager::GetDataDirString(dir, user_id));
    const auto base_export_dir = QFileDialog::getExistingDirectory(
        nullptr, QObject::tr("Select Export Directory"), QString());
    if (base_export_dir.isEmpty()) {
        return;
    }

    std::error_code ec;
    const fs::path src_path = source_dir.toStdString();
    const fs::path dst_path = fs::path(base_export_dir.toStdString()) / name.toStdString();
    fs::create_directories(dst_path, ec);
    fs::copy(src_path, dst_path,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

    if (callback) {
        callback();
    }
}

void ImportDataDir(FrontendCommon::DataManager::DataDir dir, const std::string& user_id,
                   std::function<void()> callback) {
    namespace fs = std::filesystem;
    const auto target_dir =
        QString::fromStdString(FrontendCommon::DataManager::GetDataDirString(dir, user_id));
    const auto import_dir = QFileDialog::getExistingDirectory(
        nullptr, QObject::tr("Select Import Directory"), QString());
    if (import_dir.isEmpty()) {
        return;
    }

    std::error_code ec;
    const fs::path src_path = import_dir.toStdString();
    const fs::path dst_path = target_dir.toStdString();
    fs::create_directories(dst_path, ec);
    fs::copy(src_path, dst_path,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

    if (callback) {
        callback();
    }
}

} // namespace QtCommon::Content
