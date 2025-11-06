// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include "fs.h"
#include "common/fs/ryujinx_compat.h"
#include "common/fs/symlink.h"
#include "frontend_common/data_manager.h"
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

u64 GetRyujinxSaveID(const u64 &program_id)
{
    auto path = Common::FS::GetKvdbPath();
    std::vector<Common::FS::IMEN> imens;
    Common::FS::IMENReadResult res = Common::FS::ReadKvdb(path, imens);

    if (res == Common::FS::IMENReadResult::Success) {
        // TODO: this can probably be done with std::find_if but I'm lazy
        for (const Common::FS::IMEN &imen : imens) {
            if (imen.title_id == program_id)
                return imen.save_id;
        }

        QtCommon::Frontend::Critical(
            tr("Could not find Ryujinx save data"),
            StringLookup::Lookup(StringLookup::RyujinxNoSaveId).arg(program_id, 0, 16));
    } else {
        // TODO: make this long thing a function or something
        QString caption = StringLookup::Lookup(
            static_cast<StringLookup::StringKey>((int) res + (int) StringLookup::KvdbNonexistent));
        QtCommon::Frontend::Critical(tr("Could not find Ryujinx save data"), caption);
    }

    return -1;
}

std::optional<std::pair<fs::path, fs::path> > GetEmuPaths(
    const u64 program_id, const u64 save_id, const std::string &user_id)
{
    fs::path ryu_dir = Common::FS::GetRyuSavePath(save_id);

    if (user_id.empty())
        return std::nullopt;

    std::string hex_program = fmt::format("{:016X}", program_id);
    fs::path eden_dir
        = FrontendCommon::DataManager::GetDataDir(FrontendCommon::DataManager::DataDir::Saves,
                                                  user_id)
          / hex_program;

    return std::make_pair(eden_dir, ryu_dir);
}

} // namespace QtCommon::FS
