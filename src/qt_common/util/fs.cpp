// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <filesystem>
#include "common/fs/ryujinx_compat.h"
#include "common/fs/symlink.h"
#include "fs.h"
#include "qt_common/abstract/frontend.h"
#include "qt_common/qt_string_lookup.h"

namespace fs = std::filesystem;

namespace QtCommon::FS {

void LinkRyujinx(std::filesystem::path &from, std::filesystem::path &to)
{
    std::error_code ec;

    // "ignore" errors--if the dir fails to be deleted, error handling later will handle it
    fs::remove_all(to, ec);

    if (Common::FS::CreateSymlink(from, to)) {
        QtCommon::Frontend::Information(tr("Linked Save Data"), tr("Save data has been linked."));
    } else {
        QtCommon::Frontend::Critical(
            tr("Failed to link save data"),
            tr("Could not link directory:\n\t%1\nTo:\n\t%2").arg(QString::fromStdString(from.string()), QString::fromStdString(to.string())));
    }
}

bool CheckUnlink(const fs::path &eden_dir, const fs::path &ryu_dir)
{
    bool eden_link = Common::FS::IsSymlink(eden_dir);
    bool ryu_link = Common::FS::IsSymlink(ryu_dir);

    if (!(eden_link || ryu_link))
        return false;

    auto result = QtCommon::Frontend::Warning(
        tr("Already Linked"),
        tr("This title is already linked to Ryujinx. Would you like to unlink it?"),
        QtCommon::Frontend::StandardButton::Yes | QtCommon::Frontend::StandardButton::No);

    if (result != QtCommon::Frontend::StandardButton::Yes)
        return true;

    fs::path linked;
    fs::path orig;

    if (eden_link) {
        linked = eden_dir;
        orig = ryu_dir;
    } else {
        linked = ryu_dir;
        orig = eden_dir;
    }

    linked.make_preferred();
    orig.make_preferred();

    // first cleanup the symlink/junction,
    try {
        // NB: do NOT use remove_all, as Windows treats this as a remove_all to the target,
        // NOT the junction
        fs::remove(linked);
    } catch (std::exception &e) {
        QtCommon::Frontend::Critical(
            tr("Failed to unlink old directory"),
            tr("OS returned error: %1").arg(QString::fromStdString(e.what())));
        return true;
    }

    // then COPY the other dir
    try {
        fs::copy(orig, linked, fs::copy_options::recursive);
    } catch (std::exception &e) {
        QtCommon::Frontend::Critical(
            tr("Failed to copy save data"),
            tr("OS returned error: %1").arg(QString::fromStdString(e.what())));
    }

    QtCommon::Frontend::Information(
        tr("Unlink Successful"),
        tr("Successfully unlinked Ryujinx save data. Save data has been kept intact."));

    return true;
}

const fs::path GetRyujinxSavePath(const fs::path &path_hint, const u64 &program_id)
{
    auto ryu_path = path_hint;

    auto kvdb_path = Common::FS::GetKvdbPath(ryu_path);

    if (!fs::exists(kvdb_path)) {
        using namespace QtCommon::Frontend;
        auto res = Warning(
            tr("Could not find Ryujinx installation"),
            tr("Could not find a valid Ryujinx installation. This may typically occur if you are "
               "using Ryujinx in portable mode.\n\nWould you like to manually select a portable "
               "folder to use?"), StandardButton::Yes | StandardButton::No);

        if (res == StandardButton::Yes) {
            auto selected_path = GetExistingDirectory(tr("Ryujinx Portable Location"), QDir::homePath()).toStdString();
            if (selected_path.empty())
                return fs::path{};

            ryu_path = selected_path;

            // In case the user selects the actual ryujinx installation dir INSTEAD OF
            // the portable dir
            if (fs::exists(ryu_path / "portable")) {
                ryu_path = ryu_path / "portable";
            }

            kvdb_path = Common::FS::GetKvdbPath(ryu_path);

            if (!fs::exists(kvdb_path)) {
                QtCommon::Frontend::Critical(
                    tr("Not a valid Ryujinx directory"),
                    tr("The specified directory does not contain valid Ryujinx data."));
                return fs::path{};
            }
        } else {
            return fs::path{};
        }
    }

    std::vector<Common::FS::IMEN> imens;
    Common::FS::IMENReadResult res = Common::FS::ReadKvdb(kvdb_path, imens);

    if (res == Common::FS::IMENReadResult::Success) {
        for (const Common::FS::IMEN &imen : imens) {
            if (imen.title_id == program_id)
                return Common::FS::GetRyuSavePath(ryu_path, imen.save_id);
        }

        QtCommon::Frontend::Critical(
            tr("Could not find Ryujinx save data"),
            StringLookup::Lookup(StringLookup::RyujinxNoSaveId).arg(program_id, 0, 16));
    } else {
        QString caption = LOOKUP_ENUM(res, KvdbNonexistent);
        QtCommon::Frontend::Critical(tr("Could not find Ryujinx save data"), caption);
    }

    return fs::path{};
}

} // namespace QtCommon::FS
