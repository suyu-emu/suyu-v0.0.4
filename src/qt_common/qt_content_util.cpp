// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_content_util.h"
#include "common/fs/fs.h"
#include "frontend_common/content_manager.h"
#include "frontend_common/firmware_manager.h"
#include "qt_common/qt_common.h"
#include "qt_common/qt_progress_dialog.h"
#include "qt_frontend_util.h"

#include <JlCompress.h>

namespace QtCommon::Content {

bool CheckGameFirmware(u64 program_id, QObject* parent)
{
    if (FirmwareManager::GameRequiresFirmware(program_id)
        && !FirmwareManager::CheckFirmwarePresence(*system)) {
        auto result = QtCommon::Frontend::ShowMessage(
            QMessageBox::Warning,
            "Game Requires Firmware",
            "The game you are trying to launch requires firmware to boot or to get past the "
            "opening menu. Please <a href='https://yuzu-mirror.github.io/help/quickstart'>"
            "dump and install firmware</a>, or press \"OK\" to launch anyways.",
            QMessageBox::Ok | QMessageBox::Cancel,
            parent);

        return result == QMessageBox::Ok;
    }

    return true;
}

void InstallFirmware(const QString& location, bool recursive)
{
    QtCommon::Frontend::QtProgressDialog progress(tr("Installing Firmware..."),
                                                  tr("Cancel"),
                                                  0,
                                                  100,
                                                  rootObject);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.show();

    // Declare progress callback.
    auto callback = [&](size_t total_size, size_t processed_size) {
        progress.setValue(static_cast<int>((processed_size * 100) / total_size));
        return progress.wasCanceled();
    };

    static constexpr const char* failedTitle = "Firmware Install Failed";
    static constexpr const char* successTitle = "Firmware Install Succeeded";
    QMessageBox::Icon icon;
    FirmwareInstallResult result;

    const auto ShowMessage = [&]() {
        QtCommon::Frontend::ShowMessage(icon,
                                        failedTitle,
                                        GetFirmwareInstallResultString(result));
    };

    LOG_INFO(Frontend, "Installing firmware from {}", location.toStdString());

    // Check for a reasonable number of .nca files (don't hardcode them, just see if there's some in
    // there.)
    std::filesystem::path firmware_source_path = location.toStdString();
    if (!Common::FS::IsDir(firmware_source_path)) {
        return;
    }

    std::vector<std::filesystem::path> out;
    const Common::FS::DirEntryCallable dir_callback =
        [&out](const std::filesystem::directory_entry& entry) {
            if (entry.path().has_extension() && entry.path().extension() == ".nca") {
                out.emplace_back(entry.path());
            }

            return true;
        };

    callback(100, 10);

    if (recursive) {
        Common::FS::IterateDirEntriesRecursively(firmware_source_path,
                                                 dir_callback,
                                                 Common::FS::DirEntryFilter::File);
    } else {
        Common::FS::IterateDirEntries(firmware_source_path,
                                      dir_callback,
                                      Common::FS::DirEntryFilter::File);
    }

    if (out.size() <= 0) {
        result = FirmwareInstallResult::NoNCAs;
        icon = QMessageBox::Warning;
        ShowMessage();
        return;
    }

    // Locate and erase the content of nand/system/Content/registered/*.nca, if any.
    auto sysnand_content_vdir = system->GetFileSystemController().GetSystemNANDContentDirectory();
    if (sysnand_content_vdir->IsWritable()
        && !sysnand_content_vdir->CleanSubdirectoryRecursive("registered")) {
        result = FirmwareInstallResult::FailedDelete;
        icon = QMessageBox::Critical;
        ShowMessage();
        return;
    }

    LOG_INFO(Frontend,
             "Cleaned nand/system/Content/registered folder in preparation for new firmware.");

    callback(100, 20);

    auto firmware_vdir = sysnand_content_vdir->GetDirectoryRelative("registered");

    bool success = true;
    int i = 0;
    for (const auto& firmware_src_path : out) {
        i++;
        auto firmware_src_vfile = vfs->OpenFile(firmware_src_path.generic_string(),
                                                FileSys::OpenMode::Read);
        auto firmware_dst_vfile = firmware_vdir
                                      ->CreateFileRelative(firmware_src_path.filename().string());

        if (!VfsRawCopy(firmware_src_vfile, firmware_dst_vfile)) {
            LOG_ERROR(Frontend,
                      "Failed to copy firmware file {} to {} in registered folder!",
                      firmware_src_path.generic_string(),
                      firmware_src_path.filename().string());
            success = false;
        }

        if (callback(100, 20 + static_cast<int>(((i) / static_cast<float>(out.size())) * 70.0))) {
            result = FirmwareInstallResult::FailedCorrupted;
            icon = QMessageBox::Warning;
            ShowMessage();
            return;
        }
    }

    if (!success) {
        result = FirmwareInstallResult::FailedCopy;
        icon = QMessageBox::Critical;
        ShowMessage();
        return;
    }

    // Re-scan VFS for the newly placed firmware files.
    system->GetFileSystemController().CreateFactories(*vfs);

    auto VerifyFirmwareCallback = [&](size_t total_size, size_t processed_size) {
        progress.setValue(90 + static_cast<int>((processed_size * 10) / total_size));
        return progress.wasCanceled();
    };

    auto results = ContentManager::VerifyInstalledContents(*QtCommon::system,
                                                           *QtCommon::provider,
                                                           VerifyFirmwareCallback,
                                                           true);

    if (results.size() > 0) {
        const auto failed_names = QString::fromStdString(
            fmt::format("{}", fmt::join(results, "\n")));
        progress.close();
        QtCommon::Frontend::Critical(tr("Firmware integrity verification failed!"),
                                     tr("Verification failed for the following files:\n\n%1")
                                         .arg(failed_names));
        return;
    }

    progress.close();

    const auto pair = FirmwareManager::GetFirmwareVersion(*system);
    const auto firmware_data = pair.first;
    const std::string display_version(firmware_data.display_version.data());

    result = FirmwareInstallResult::Success;
    QtCommon::Frontend::Information(rootObject,
                                    tr(successTitle),
                                    tr(GetFirmwareInstallResultString(result))
                                        .arg(QString::fromStdString(display_version)));
}

QString UnzipFirmwareToTmp(const QString& location)
{
    namespace fs = std::filesystem;
    fs::path tmp{fs::temp_directory_path()};

    if (!fs::create_directories(tmp / "eden" / "firmware")) {
        return "";
    }

    tmp /= "eden";
    tmp /= "firmware";

    QString qCacheDir = QString::fromStdString(tmp.string());

    QFile zip(location);

    QStringList result = JlCompress::extractDir(&zip, qCacheDir);
    if (result.isEmpty()) {
        return "";
    }

    return qCacheDir;
}

// Content //
void VerifyGameContents(const std::string& game_path)
{
    QtCommon::Frontend::QtProgressDialog progress(tr("Verifying integrity..."),
                                                  tr("Cancel"),
                                                  0,
                                                  100,
                                                  rootObject);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);
    progress.setAutoClose(false);
    progress.setAutoReset(false);

    const auto callback = [&](size_t total_size, size_t processed_size) {
        progress.setValue(static_cast<int>((processed_size * 100) / total_size));
        return progress.wasCanceled();
    };

    const auto result = ContentManager::VerifyGameContents(*system, game_path, callback);

    switch (result) {
    case ContentManager::GameVerificationResult::Success:
        QtCommon::Frontend::Information(rootObject,
                                        tr("Integrity verification succeeded!"),
                                        tr("The operation completed successfully."));
        break;
    case ContentManager::GameVerificationResult::Failed:
        QtCommon::Frontend::Critical(rootObject,
                                     tr("Integrity verification failed!"),
                                     tr("File contents may be corrupt or missing."));
        break;
    case ContentManager::GameVerificationResult::NotImplemented:
        QtCommon::Frontend::Warning(
            rootObject,
            tr("Integrity verification couldn't be performed"),
            tr("Firmware installation cancelled, firmware may be in a bad state or corrupted. "
               "File contents could not be checked for validity."));
    }
}

void InstallKeys()
{
    const QString key_source_location
        = QtCommon::Frontend::GetOpenFileName(tr("Select Dumped Keys Location"),
                                              {},
                                              QStringLiteral("Decryption Keys (*.keys)"),
                                              {},
                                              QtCommon::Frontend::Option::ReadOnly);

    if (key_source_location.isEmpty()) {
        return;
    }

    FirmwareManager::KeyInstallResult result = FirmwareManager::InstallKeys(key_source_location
                                                                                .toStdString(),
                                                                            "keys");

    system->GetFileSystemController().CreateFactories(*QtCommon::vfs);

    switch (result) {
    case FirmwareManager::KeyInstallResult::Success:
        QtCommon::Frontend::Information(tr("Decryption Keys install succeeded"),
                                        tr("Decryption Keys were successfully installed"));
        break;
    default:
        QtCommon::Frontend::Critical(tr("Decryption Keys install failed"),
                                     tr(FirmwareManager::GetKeyInstallResultString(result)));
        break;
    }
}

void VerifyInstalledContents() {
    // Initialize a progress dialog.
    QtCommon::Frontend::QtProgressDialog progress(tr("Verifying integrity..."), tr("Cancel"), 0, 100, QtCommon::rootObject);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);
    progress.setAutoClose(false);
    progress.setAutoReset(false);

    // Declare progress callback.
    auto QtProgressCallback = [&](size_t total_size, size_t processed_size) {
        progress.setValue(static_cast<int>((processed_size * 100) / total_size));
        return progress.wasCanceled();
    };

    const std::vector<std::string> result =
        ContentManager::VerifyInstalledContents(*QtCommon::system, *QtCommon::provider, QtProgressCallback);
    progress.close();

    if (result.empty()) {
        QtCommon::Frontend::Information(tr("Integrity verification succeeded!"),
                                        tr("The operation completed successfully."));
    } else {
        const auto failed_names =
            QString::fromStdString(fmt::format("{}", fmt::join(result, "\n")));
        QtCommon::Frontend::Critical(
            tr("Integrity verification failed!"),
            tr("Verification failed for the following files:\n\n%1").arg(failed_names));
    }
}

} // namespace QtCommon::Content
