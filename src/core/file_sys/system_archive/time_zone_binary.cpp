// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <span>
#include <vector>

#include "common/swap.h"
#include "core/file_sys/system_archive/time_zone_binary.h"
#include "core/file_sys/vfs/vfs_static.h"
#include "core/file_sys/vfs/vfs_types.h"
#include "core/file_sys/vfs/vfs_vector.h"

#include "nx_tzdb.h"

namespace FileSys::SystemArchive {

VirtualDir TimeZoneBinary() {
    std::vector<VirtualDir> america_sub_dirs;
    america_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_america_argentina(), std::vector<VirtualDir>{}, "Argentina"));
    america_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_america_indiana(), std::vector<VirtualDir>{}, "Indiana"));
    america_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_america_kentucky(), std::vector<VirtualDir>{}, "Kentucky"));
    america_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_america_north_dakota(), std::vector<VirtualDir>{}, "North_Dakota"));
    std::vector<VirtualDir> zoneinfo_sub_dirs;
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_africa(), std::vector<VirtualDir>{}, "Africa"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_america(), std::move(america_sub_dirs), "America"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_antarctica(), std::vector<VirtualDir>{}, "Antarctica"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_arctic(), std::vector<VirtualDir>{}, "Arctic"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_asia(), std::vector<VirtualDir>{}, "Asia"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_atlantic(), std::vector<VirtualDir>{}, "Atlantic"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_australia(), std::vector<VirtualDir>{}, "Australia"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_brazil(), std::vector<VirtualDir>{}, "Brazil"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_canada(), std::vector<VirtualDir>{}, "Canada"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_chile(), std::vector<VirtualDir>{}, "Chile"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_etc(), std::vector<VirtualDir>{}, "Etc"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_europe(), std::vector<VirtualDir>{}, "Europe"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_indian(), std::vector<VirtualDir>{}, "Indian"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_mexico(), std::vector<VirtualDir>{}, "Mexico"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_pacific(), std::vector<VirtualDir>{}, "Pacific"));
    zoneinfo_sub_dirs.push_back(std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_us(), std::vector<VirtualDir>{}, "US"));
    std::vector<VirtualDir> zoneinfo_dir{std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_zoneinfo(), std::move(zoneinfo_sub_dirs), "zoneinfo")};
    // last files (root)
    return std::make_shared<VectorVfsDirectory>(NxTzdb::CollectFiles_base(), std::move(zoneinfo_dir), "data");
}

} // namespace FileSys::SystemArchive
