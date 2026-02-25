// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <JlCompress.h>
#include "frontend_common/mod_manager.h"
#include "mod.h"
#include "qt_common/abstract/frontend.h"

namespace QtCommon::Mod {
QStringList GetModFolders(const QString& root, const QString& fallbackName) {
    namespace fs = std::filesystem;

    const auto std_root = root.toStdString();

    auto paths = FrontendCommon::GetModFolder(std_root);

    // multi mod zip
    if (paths.size() > 1) {
        // We just have to assume it's properly formed here.
        // If not, you're out of luck.
        QStringList qpaths;
        for (const fs::path& path : paths) {
            qpaths << QString::fromStdString(path.string());
        }

        return qpaths;
    }
    // either frontend didn't detect any romfs/exefs, or is a single-mod zip
    else {
        fs::path std_path;
        if (!paths.empty())
            std_path = paths[0];

        QString default_name;
        if (!fallbackName.isEmpty())
            default_name = fallbackName;
        else if (!paths.empty())
            default_name = QString::fromStdString(std_path.filename().string());
        else
            default_name = root.split(QLatin1Char('/')).last();

        QString name = QtCommon::Frontend::GetTextInput(
            tr("Mod Name"), tr("What should this mod be called?"), default_name);

        if (name.isEmpty()) return {};

        // if std_path is empty, frontend_common could not determine mod type and/or name.
        // so we have to prompt the user and set up the structure ourselves
        if (paths.empty()) {
            // TODO: Carboxyl impl.
            const QStringList choices = {
                tr("RomFS"),
                tr("ExeFS/Patch"),
                tr("Cheat"),
            };

            int choice = QtCommon::Frontend::Choice(
                tr("Mod Type"),
                tr("Could not detect mod type automatically. Please manually "
                   "specify the type of mod you downloaded.\n\nMost mods are RomFS mods, but "
                   "patches "
                   "(.pchtxt) are typically ExeFS mods."),
                choices);

            std::string to_make;

            switch (choice) {
            case 0:
                to_make = "romfs";
                break;
            case 1:
                to_make = "exefs";
                break;
            case 2:
                to_make = "cheats";
                break;
            default:
                return {};
            }

            // now make a temp directory...
            const auto mod_dir = fs::temp_directory_path() / "eden" / "mod" / name.toStdString();
            const auto tmp = mod_dir / to_make;
            fs::remove_all(mod_dir);
            if (!fs::create_directories(tmp)) {
                LOG_ERROR(Frontend, "Failed to create temporary directory {}", tmp.string());
                return {};
            }

            std_path = mod_dir;

            // ... and copy everything from the root to the temp dir
            for (const auto& entry : fs::directory_iterator(root.toStdString())) {
                const auto target = tmp / entry.path().filename();

                fs::copy(entry.path(), target,
                         fs::copy_options::recursive | fs::copy_options::overwrite_existing);
            }
        } else {
            // Rename the existing mod folder.
            const auto new_path = std_path.parent_path() / name.toStdString();
            fs::remove_all(new_path);
            fs::rename(std_path, new_path);
            std_path = new_path;
        }

        return {QString::fromStdString(std_path.string())};
    }
}

// TODO(crueter): Make this a common extract_to_tmp func
const QString ExtractMod(const QString& path) {
    namespace fs = std::filesystem;
    fs::path tmp{fs::temp_directory_path() / "eden" / "unzip_mod"};

    fs::remove_all(tmp);
    if (!fs::create_directories(tmp)) {
        QtCommon::Frontend::Critical(tr("Mod Extract Failed"),
                                     tr("Failed to create temporary directory %1")
                                         .arg(QString::fromStdString(tmp.string())));
        return QString();
    }

    QString qCacheDir = QString::fromStdString(tmp.string());

    QFile zip{path};

    // TODO(crueter): use QtCompress
    QStringList result = JlCompress::extractDir(&zip, qCacheDir);
    if (result.isEmpty()) {
        QtCommon::Frontend::Critical(tr("Mod Extract Failed"),
                                     tr("Zip file %1 is empty").arg(path));
        return QString();
    }

    return qCacheDir;
}

} // namespace QtCommon::Mod
