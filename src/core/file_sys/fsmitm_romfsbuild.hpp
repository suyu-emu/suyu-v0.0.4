/*
 * Copyright (c) 2018 Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Adapted by DarkLordZach for use/interaction with yuzu
 *
 * Modifications Copyright 2018 yuzu emulator team
 * Licensed under GPLv2 or any later version
 * Refer to the license.txt file included.
 */

#pragma once
#include <map>
#include <boost/detail/container_fwd.hpp>
#include "common/common_types.h"
#include "vfs.h"

namespace FileSys {

/* Used as comparator for std::map<char *, RomFSBuild*Context> */
struct build_ctx_cmp {
    bool operator()(const char* a, const char* b) const {
        return strcmp(a, b) < 0;
    }
};

struct RomFSDirectoryEntry;
struct RomFSFileEntry;
struct RomFSBuildDirectoryContext;
struct RomFSBuildFileContext;

class RomFSBuildContext {
private:
    VirtualDir base;
    RomFSBuildDirectoryContext* root;
    std::map<char*, RomFSBuildDirectoryContext*, build_ctx_cmp> directories;
    std::map<char*, RomFSBuildFileContext*, build_ctx_cmp> files;
    u64 num_dirs = 0;
    u64 num_files = 0;
    u64 dir_table_size = 0;
    u64 file_table_size = 0;
    u64 dir_hash_table_size = 0;
    u64 file_hash_table_size = 0;
    u64 file_partition_size = 0;

    void VisitDirectory(VirtualDir filesys, RomFSBuildDirectoryContext* parent);

    bool AddDirectory(RomFSBuildDirectoryContext* parent_dir_ctx,
                      RomFSBuildDirectoryContext* dir_ctx,
                      RomFSBuildDirectoryContext** out_dir_ctx);
    bool AddFile(RomFSBuildDirectoryContext* parent_dir_ctx, RomFSBuildFileContext* file_ctx);

public:
    explicit RomFSBuildContext(VirtualDir base);

    /* This finalizes the context. */
    std::map<u64, VirtualFile> Build();
};

static inline RomFSDirectoryEntry* romfs_get_direntry(void* directories, uint32_t offset) {
    return (RomFSDirectoryEntry*)((uintptr_t)directories + offset);
}

static inline RomFSFileEntry* romfs_get_fentry(void* files, uint32_t offset) {
    return (RomFSFileEntry*)((uintptr_t)files + offset);
}

static inline uint32_t romfs_calc_path_hash(uint32_t parent, const unsigned char* path,
                                            uint32_t start, size_t path_len) {
    uint32_t hash = parent ^ 123456789;
    for (uint32_t i = 0; i < path_len; i++) {
        hash = (hash >> 5) | (hash << 27);
        hash ^= path[start + i];
    }

    return hash;
}

static inline uint32_t romfs_get_hash_table_count(uint32_t num_entries) {
    if (num_entries < 3) {
        return 3;
    } else if (num_entries < 19) {
        return num_entries | 1;
    }
    uint32_t count = num_entries;
    while (count % 2 == 0 || count % 3 == 0 || count % 5 == 0 || count % 7 == 0 ||
           count % 11 == 0 || count % 13 == 0 || count % 17 == 0) {
        count++;
    }
    return count;
}

} // namespace FileSys
