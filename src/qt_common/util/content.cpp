// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_common/util/content.h"
#include "qt_common/util/game.h"

#include "common/fs/fs.h"
#include "core/hle/service/acc/profile_manager.h"
#include "frontend_common/content_manager.h"
#include "frontend_common/data_manager.h"
#include "frontend_common/firmware_manager.h"

#include "compress.h"
#include "qt_common/abstract/frontend.h"
#include "qt_common/abstract/qt_progress_dialog.h"
#include "qt_common/qt_common.h"

#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <JlCompress.h>

namespace QtCommon::Content {

bool CheckGameFirmware(u64 program_id, QObject* parent)
{
    if (FirmwareManager::GameRequiresFirmware(program_id)
        && !FirmwareManager::CheckFirmwarePresence(*system)) {
        auto result = QtCommon::Frontend::ShowMessage(
            QMessageBox::Warning,
            tr("Game Requires Firmware"),
            tr("The game you are trying to launch requires firmware to boot or to get past the "
               "opening menu. Please <a href='https://yuzu-mirror.github.io/help/quickstart'>"
               "dump and install firmware</a>, or press \"OK\" to launch anyways."),
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

    QString failedTitle = tr("Firmware Install Failed");
    QString successTitle = tr("Firmware Install Succeeded");
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
        auto firmware_dst_vfile = firmware_vdir->CreateFileRelative(
            firmware_src_path.filename().string());

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
        QtCommon::Frontend::Critical(
            tr("Firmware integrity verification failed!"),
            tr("Verification failed for the following files:\n\n%1").arg(failed_names));
        return;
    }

    progress.close();

    const auto pair = FirmwareManager::GetFirmwareVersion(*system);
    const auto firmware_data = pair.first;
    const std::string display_version(firmware_data.display_version.data());

    result = FirmwareInstallResult::Success;
    QtCommon::Frontend::Information(successTitle,
                                    GetFirmwareInstallResultString(result).arg(
                                        QString::fromStdString(display_version)));
}

QString UnzipFirmwareToTmp(const QString& location)
{
    namespace fs = std::filesystem;
    fs::path tmp{fs::temp_directory_path()};

    if (!fs::create_directories(tmp / "eden" / "firmware")) {
        return QString();
    }

    tmp /= "eden";
    tmp /= "firmware";

    QString qCacheDir = QString::fromStdString(tmp.string());

    QFile zip(location);

    // TODO(crueter): use QtCompress
    QStringList result = JlCompress::extractDir(&zip, qCacheDir);
    if (result.isEmpty()) {
        return QString();
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

    FirmwareManager::KeyInstallResult result
        = FirmwareManager::InstallKeys(key_source_location.toStdString(), "keys");

    system->GetFileSystemController().CreateFactories(*QtCommon::vfs);

    const QString resMsg = GetKeyInstallResultString(result);
    switch (result) {
    case FirmwareManager::KeyInstallResult::Success:
        QtCommon::Frontend::Information(tr("Decryption Keys install succeeded"), resMsg);
        break;
    default:
        QtCommon::Frontend::Critical(tr("Decryption Keys install failed"), resMsg);
        break;
    }
}

void VerifyInstalledContents()
{
    // Initialize a progress dialog.
    QtCommon::Frontend::QtProgressDialog progress(tr("Verifying integrity..."),
                                                  tr("Cancel"),
                                                  0,
                                                  100,
                                                  rootObject);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);
    progress.setAutoClose(false);
    progress.setAutoReset(false);

    // Declare progress callback.
    auto QtProgressCallback = [&](size_t total_size, size_t processed_size) {
        progress.setValue(static_cast<int>((processed_size * 100) / total_size));
        return progress.wasCanceled();
    };

    const std::vector<std::string> result
        = ContentManager::VerifyInstalledContents(*QtCommon::system,
                                                  *QtCommon::provider,
                                                  QtProgressCallback);
    progress.close();

    if (result.empty()) {
        QtCommon::Frontend::Information(tr("Integrity verification succeeded!"),
                                        tr("The operation completed successfully."));
    } else {
        const auto failed_names = QString::fromStdString(fmt::format("{}", fmt::join(result, "\n")));
        QtCommon::Frontend::Critical(
            tr("Integrity verification failed!"),
            tr("Verification failed for the following files:\n\n%1").arg(failed_names));
    }
}

void FixProfiles()
{
    // Reset user save files after config is initialized and migration is done.
    // Doing it at init time causes profiles to read from the wrong place entirely if NAND dir is not default
    // TODO: better solution
    system->GetProfileManager().ResetUserSaveFile();
    std::vector<std::string> orphaned = system->GetProfileManager().FindOrphanedProfiles();
    std::vector<std::string> good = system->GetProfileManager().FindGoodProfiles();

    // no orphaned dirs--all good :)
    if (orphaned.empty())
        return;

    // otherwise, let the user know
    QString qorphaned;

    // max. of 8 orphaned profiles is fair, I think
    // 36 = 32 (UUID) + 4 (<br>)
    qorphaned.reserve(8 * 36);

    for (const std::string& s : orphaned) {
        qorphaned = qorphaned % QStringLiteral("<br>") % QString::fromStdString(s);
    }

    QString qgood;

    // max. of 8 good profiles is fair, I think
    // 36 = 32 (UUID) + 4 (<br>)
    qgood.reserve(8 * 36);

    for (const std::string& s : good) {
        qgood = qgood % QStringLiteral("<br>") % QString::fromStdString(s);
    }

    QtCommon::Frontend::Critical(
        tr("Orphaned Profiles Detected!"),
        tr("UNEXPECTED BAD THINGS MAY HAPPEN IF YOU DON'T READ THIS!<br>"
           "Eden has detected the following save directories with no attached profile:<br>"
           "%1<br><br>"
           "The following profiles are valid:<br>"
           "%2<br><br>"
           "Click \"OK\" to open your save folder and fix up your profiles.<br>"
           "Hint: copy the contents of the largest or last-modified folder elsewhere, "
           "delete all orphaned profiles, and move your copied contents to the good profile.<br><br>"
           "Still confused? See the <a href='https://git.eden-emu.dev/eden-emu/eden/src/branch/master/docs/user/Orphaned.md'>help page</a>.<br>")
            .arg(qorphaned, qgood));

    QtCommon::Game::OpenSaveFolder();
}

void ClearDataDir(FrontendCommon::DataManager::DataDir dir, const std::string& user_id)
{
    auto result = QtCommon::Frontend::Warning(tr("Really clear data?"),
                                              tr("Important data may be lost!"),
                                              QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes)
        return;

    result = QtCommon::Frontend::Warning(
        tr("Are you REALLY sure?"),
        tr("Once deleted, your data will NOT come back!\n"
           "Only do this if you're 100% sure you want to delete this data."),
        QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes)
        return;

    QtCommon::Frontend::QtProgressDialog dialog(tr("Clearing..."), QString(), 0, 0);
    dialog.show();

    FrontendCommon::DataManager::ClearDir(dir, user_id);

    dialog.close();
}

void ExportDataDir(FrontendCommon::DataManager::DataDir data_dir,
                   const std::string& user_id,
                   const QString& name,
                   std::function<void()> callback)
{
    using namespace QtCommon::Frontend;
    const std::string dir = FrontendCommon::DataManager::GetDataDirString(data_dir, user_id);

    const QString zip_dump_location = GetSaveFileName(tr("Select Export Location"),
                                                      tr("%1.zip").arg(name),
                                                      tr("Zipped Archives (*.zip)"));

    if (zip_dump_location.isEmpty())
        return;

    QtProgressDialog* progress = new QtProgressDialog(
        tr("Exporting data. This may take a while..."), tr("Cancel"), 0, 100, rootObject);

    progress->setWindowTitle(tr("Exporting"));
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(100);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->show();

    QGuiApplication::processEvents();

    auto progress_callback = [=](size_t total_size, size_t processed_size) {
        QMetaObject::invokeMethod(progress,
                                  "setValue",
                                  Qt::DirectConnection,
                                  Q_ARG(int, static_cast<int>((processed_size * 100) / total_size)));
        return !progress->wasCanceled();
    };

    QFuture<bool> future = QtConcurrent::run([=]() {
        return QtCommon::Compress::compressDir(zip_dump_location,
                                               QString::fromStdString(dir),
                                               QtCommon::Compress::Options(),
                                               progress_callback);
    });

    QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(rootObject);

    QObject::connect(watcher, &QFutureWatcher<bool>::finished, rootObject, [=]() {
        progress->close();

        if (watcher->result()) {
            Information(tr("Exported Successfully"), tr("Data was exported successfully."));
        } else if (progress->wasCanceled()) {
            Information(tr("Export Cancelled"), tr("Export was cancelled by the user."));
        } else {
            Critical(
                tr("Export Failed"),
                tr("Ensure you have write permissions on the targeted directory and try again."));
        }

        progress->deleteLater();
        watcher->deleteLater();
        if (callback)
            callback();
    });

    watcher->setFuture(future);
}

void ImportDataDir(FrontendCommon::DataManager::DataDir data_dir,
                   const std::string& user_id,
                   std::function<void()> callback)
{
    const std::string dir = FrontendCommon::DataManager::GetDataDirString(data_dir, user_id);

    using namespace QtCommon::Frontend;

    const QString zip_dump_location = GetOpenFileName(tr("Select Import Location"),
                                                      {},
                                                      tr("Zipped Archives (*.zip)"));

    if (zip_dump_location.isEmpty())
        return;

    StandardButton button = Warning(
        tr("Import Warning"),
        tr("All previous data in this directory will be deleted. Are you sure you wish to "
           "proceed?"),
        StandardButton::Yes | StandardButton::No);

    if (button != QMessageBox::Yes)
        return;

    QtProgressDialog* progress = new QtProgressDialog(
        tr("Importing data. This may take a while..."), tr("Cancel"), 0, 100, rootObject);

    progress->setWindowTitle(tr("Importing"));
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(100);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->show();
    progress->setValue(0);

    QGuiApplication::processEvents();

    // to prevent GUI mangling we have to run this in a thread as well
    QFuture<bool> delete_future = QtConcurrent::run([=]() {
        FrontendCommon::DataManager::ClearDir(data_dir, user_id);
        return !progress->wasCanceled();
    });

    QFutureWatcher<bool>* delete_watcher = new QFutureWatcher<bool>(rootObject);
    delete_watcher->setFuture(delete_future);

    QObject::connect(delete_watcher, &QFutureWatcher<bool>::finished, rootObject, [=]() {
        auto progress_callback = [=](size_t total_size, size_t processed_size) {
            QMetaObject::invokeMethod(progress,
                                      "setValue",
                                      Qt::DirectConnection,
                                      Q_ARG(int,
                                            static_cast<int>((processed_size * 100) / total_size)));
            return !progress->wasCanceled();
        };

        QFuture<bool> future = QtConcurrent::run([=]() {
            return !QtCommon::Compress::extractDir(zip_dump_location,
                                                   QString::fromStdString(dir),
                                                   progress_callback)
                        .empty();
        });

        QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(rootObject);

        QObject::connect(watcher, &QFutureWatcher<bool>::finished, rootObject, [=]() {
            progress->close();

            // this sucks
            if (watcher->result()) {
                Information(tr("Imported Successfully"), tr("Data was imported successfully."));
            } else if (progress->wasCanceled()) {
                Information(tr("Import Cancelled"), tr("Import was cancelled by the user."));
            } else {
                Critical(tr("Import Failed"),
                         tr("Ensure you have read permissions on the targeted directory and try "
                            "again."));
            }

            progress->deleteLater();
            watcher->deleteLater();
            if (callback)
                callback();
        });

        watcher->setFuture(future);
    });
}

} // namespace QtCommon::Content
