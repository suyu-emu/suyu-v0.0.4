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

#include <cstring>
#include "common/assert.h"
#include "fsmitm_romfsbuild.hpp"
#include "vfs.h"
#include "vfs_vector.h"

namespace FileSys {

constexpr u64 FS_MAX_PATH = 0x301;

constexpr u32 ROMFS_ENTRY_EMPTY = 0xFFFFFFFF;
constexpr u32 ROMFS_FILEPARTITION_OFS = 0x200;

/* Types for building a RomFS. */
struct RomFSHeader {
    u64 header_size;
    u64 dir_hash_table_ofs;
    u64 dir_hash_table_size;
    u64 dir_table_ofs;
    u64 dir_table_size;
    u64 file_hash_table_ofs;
    u64 file_hash_table_size;
    u64 file_table_ofs;
    u64 file_table_size;
    u64 file_partition_ofs;
};
static_assert(sizeof(RomFSHeader) == 0x50, "RomFSHeader has incorrect size.");

struct RomFSDirectoryEntry {
    u32 parent;
    u32 sibling;
    u32 child;
    u32 file;
    u32 hash;
    u32 name_size;
    char name[];
};
static_assert(sizeof(RomFSDirectoryEntry) == 0x18, "RomFSDirectoryEntry has incorrect size.");

struct RomFSFileEntry {
    u32 parent;
    u32 sibling;
    u64 offset;
    u64 size;
    u32 hash;
    u32 name_size;
    char name[];
};
static_assert(sizeof(RomFSFileEntry) == 0x20, "RomFSFileEntry has incorrect size.");

struct RomFSBuildFileContext;

struct RomFSBuildDirectoryContext {
    char* path;
    u32 cur_path_ofs;
    u32 path_len;
    u32 entry_offset = 0;
    RomFSBuildDirectoryContext* parent = nullptr;
    RomFSBuildDirectoryContext* child = nullptr;
    RomFSBuildDirectoryContext* sibling = nullptr;
    RomFSBuildFileContext* file = nullptr;
};

struct RomFSBuildFileContext {
    char* path;
    u32 cur_path_ofs;
    u32 path_len;
    u32 entry_offset = 0;
    u64 offset = 0;
    u64 size = 0;
    RomFSBuildDirectoryContext* parent = nullptr;
    RomFSBuildFileContext* sibling = nullptr;
    VirtualFile source = nullptr;
};

void RomFSBuildContext::VisitDirectory(VirtualDir root_romfs, RomFSBuildDirectoryContext* parent) {
    std::vector<RomFSBuildDirectoryContext*> child_dirs;

    VirtualDir dir;

    if (parent->path_len == 0)
        dir = root_romfs;
    else
        dir = root_romfs->GetDirectoryRelative(parent->path);

    const auto entries = dir->GetEntries();

    for (const auto& kv : entries) {
        if (kv.second == VfsEntryType::Directory) {
            RomFSBuildDirectoryContext* child = new RomFSBuildDirectoryContext({0});
            /* Set child's path. */
            child->cur_path_ofs = parent->path_len + 1;
            child->path_len = child->cur_path_ofs + kv.first.size();
            child->path = new char[child->path_len + 1];
            strcpy(child->path, parent->path);
            ASSERT(child->path_len < FS_MAX_PATH);
            strcat(child->path + parent->path_len, "/");
            strcat(child->path + parent->path_len, kv.first.c_str());

            if (!this->AddDirectory(parent, child, nullptr)) {
                delete child->path;
                delete child;
            } else {
                child_dirs.push_back(child);
            }
        } else {
            RomFSBuildFileContext* child = new RomFSBuildFileContext({0});
            /* Set child's path. */
            child->cur_path_ofs = parent->path_len + 1;
            child->path_len = child->cur_path_ofs + kv.first.size();
            child->path = new char[child->path_len + 1];
            strcpy(child->path, parent->path);
            ASSERT(child->path_len < FS_MAX_PATH);
            strcat(child->path + parent->path_len, "/");
            strcat(child->path + parent->path_len, kv.first.c_str());

            child->source = root_romfs->GetFileRelative(child->path);

            child->size = child->source->GetSize();

            if (!this->AddFile(parent, child)) {
                delete child->path;
                delete child;
            }
        }
    }

    for (auto& child : child_dirs) {
        this->VisitDirectory(root_romfs, child);
    }
}

bool RomFSBuildContext::AddDirectory(RomFSBuildDirectoryContext* parent_dir_ctx,
                                     RomFSBuildDirectoryContext* dir_ctx,
                                     RomFSBuildDirectoryContext** out_dir_ctx) {
    /* Check whether it's already in the known directories. */
    auto existing = this->directories.find(dir_ctx->path);
    if (existing != this->directories.end()) {
        if (out_dir_ctx) {
            *out_dir_ctx = existing->second;
        }
        return false;
    }

    /* Add a new directory. */
    this->num_dirs++;
    this->dir_table_size +=
        sizeof(RomFSDirectoryEntry) + ((dir_ctx->path_len - dir_ctx->cur_path_ofs + 3) & ~3);
    dir_ctx->parent = parent_dir_ctx;
    this->directories.insert({dir_ctx->path, dir_ctx});

    if (out_dir_ctx) {
        *out_dir_ctx = dir_ctx;
    }
    return true;
}

bool RomFSBuildContext::AddFile(RomFSBuildDirectoryContext* parent_dir_ctx,
                                RomFSBuildFileContext* file_ctx) {
    /* Check whether it's already in the known files. */
    auto existing = this->files.find(file_ctx->path);
    if (existing != this->files.end()) {
        return false;
    }

    /* Add a new file. */
    this->num_files++;
    this->file_table_size +=
        sizeof(RomFSFileEntry) + ((file_ctx->path_len - file_ctx->cur_path_ofs + 3) & ~3);
    file_ctx->parent = parent_dir_ctx;
    this->files.insert({file_ctx->path, file_ctx});

    return true;
}

RomFSBuildContext::RomFSBuildContext(VirtualDir base_) : base(std::move(base_)) {
    this->root = new RomFSBuildDirectoryContext({0});
    this->root->path = new char[1];
    this->root->path[0] = '\x00';
    this->directories.insert({this->root->path, this->root});
    this->num_dirs = 1;
    this->dir_table_size = 0x18;

    VisitDirectory(base, this->root);
}

std::map<u64, VirtualFile> RomFSBuildContext::Build() {
    std::map<u64, VirtualFile> out;
    RomFSBuildFileContext* cur_file;
    RomFSBuildDirectoryContext* cur_dir;

    const auto dir_hash_table_entry_count = romfs_get_hash_table_count(this->num_dirs);
    const auto file_hash_table_entry_count = romfs_get_hash_table_count(this->num_files);
    this->dir_hash_table_size = 4 * dir_hash_table_entry_count;
    this->file_hash_table_size = 4 * file_hash_table_entry_count;

    /* Assign metadata pointers */
    RomFSHeader* header = new RomFSHeader({0});
    auto metadata = new u8[this->dir_hash_table_size + this->dir_table_size +
                           this->file_hash_table_size + this->file_table_size];
    auto dir_hash_table = reinterpret_cast<u32*>(metadata);
    const auto dir_table = reinterpret_cast<RomFSDirectoryEntry*>(
        reinterpret_cast<uintptr_t>(dir_hash_table) + this->dir_hash_table_size);
    auto file_hash_table =
        reinterpret_cast<u32*>(reinterpret_cast<uintptr_t>(dir_table) + this->dir_table_size);
    const auto file_table = reinterpret_cast<RomFSFileEntry*>(
        reinterpret_cast<uintptr_t>(file_hash_table) + this->file_hash_table_size);

    /* Clear out hash tables. */
    for (u32 i = 0; i < dir_hash_table_entry_count; i++)
        dir_hash_table[i] = ROMFS_ENTRY_EMPTY;
    for (u32 i = 0; i < file_hash_table_entry_count; i++)
        file_hash_table[i] = ROMFS_ENTRY_EMPTY;

    out.clear();

    /* Determine file offsets. */
    u32 entry_offset = 0;
    RomFSBuildFileContext* prev_file = nullptr;
    for (const auto& it : this->files) {
        cur_file = it.second;
        this->file_partition_size = (this->file_partition_size + 0xFULL) & ~0xFULL;
        cur_file->offset = this->file_partition_size;
        this->file_partition_size += cur_file->size;
        cur_file->entry_offset = entry_offset;
        entry_offset +=
            sizeof(RomFSFileEntry) + ((cur_file->path_len - cur_file->cur_path_ofs + 3) & ~3);
        prev_file = cur_file;
    }
    /* Assign deferred parent/sibling ownership. */
    for (auto it = this->files.rbegin(); it != this->files.rend(); it++) {
        cur_file = it->second;
        cur_file->sibling = cur_file->parent->file;
        cur_file->parent->file = cur_file;
    }

    /* Determine directory offsets. */
    entry_offset = 0;
    for (const auto& it : this->directories) {
        cur_dir = it.second;
        cur_dir->entry_offset = entry_offset;
        entry_offset +=
            sizeof(RomFSDirectoryEntry) + ((cur_dir->path_len - cur_dir->cur_path_ofs + 3) & ~3);
    }
    /* Assign deferred parent/sibling ownership. */
    for (auto it = this->directories.rbegin(); it->second != this->root; it++) {
        cur_dir = it->second;
        cur_dir->sibling = cur_dir->parent->child;
        cur_dir->parent->child = cur_dir;
    }

    /* Populate file tables. */
    for (const auto& it : this->files) {
        cur_file = it.second;
        RomFSFileEntry* cur_entry = romfs_get_fentry(file_table, cur_file->entry_offset);

        cur_entry->parent = cur_file->parent->entry_offset;
        cur_entry->sibling =
            (cur_file->sibling == nullptr) ? ROMFS_ENTRY_EMPTY : cur_file->sibling->entry_offset;
        cur_entry->offset = cur_file->offset;
        cur_entry->size = cur_file->size;

        const auto name_size = cur_file->path_len - cur_file->cur_path_ofs;
        const auto hash = romfs_calc_path_hash(cur_file->parent->entry_offset,
                                               reinterpret_cast<unsigned char*>(cur_file->path) +
                                                   cur_file->cur_path_ofs,
                                               0, name_size);
        cur_entry->hash = file_hash_table[hash % file_hash_table_entry_count];
        file_hash_table[hash % file_hash_table_entry_count] = cur_file->entry_offset;

        cur_entry->name_size = name_size;
        memset(cur_entry->name, 0, (cur_entry->name_size + 3) & ~3);
        memcpy(cur_entry->name, cur_file->path + cur_file->cur_path_ofs, name_size);

        out.emplace(cur_file->offset + ROMFS_FILEPARTITION_OFS, cur_file->source);
    }

    /* Populate dir tables. */
    for (const auto& it : this->directories) {
        cur_dir = it.second;
        RomFSDirectoryEntry* cur_entry = romfs_get_direntry(dir_table, cur_dir->entry_offset);
        cur_entry->parent = cur_dir == this->root ? 0 : cur_dir->parent->entry_offset;
        cur_entry->sibling =
            (cur_dir->sibling == nullptr) ? ROMFS_ENTRY_EMPTY : cur_dir->sibling->entry_offset;
        cur_entry->child =
            (cur_dir->child == nullptr) ? ROMFS_ENTRY_EMPTY : cur_dir->child->entry_offset;
        cur_entry->file =
            (cur_dir->file == nullptr) ? ROMFS_ENTRY_EMPTY : cur_dir->file->entry_offset;

        u32 name_size = cur_dir->path_len - cur_dir->cur_path_ofs;
        u32 hash = romfs_calc_path_hash(
            cur_dir == this->root ? 0 : cur_dir->parent->entry_offset,
            reinterpret_cast<unsigned char*>(cur_dir->path) + cur_dir->cur_path_ofs, 0, name_size);
        cur_entry->hash = dir_hash_table[hash % dir_hash_table_entry_count];
        dir_hash_table[hash % dir_hash_table_entry_count] = cur_dir->entry_offset;

        cur_entry->name_size = name_size;
        memset(cur_entry->name, 0, (cur_entry->name_size + 3) & ~3);
        memcpy(cur_entry->name, cur_dir->path + cur_dir->cur_path_ofs, name_size);
    }

    /* Delete directories. */
    for (const auto& it : this->directories) {
        cur_dir = it.second;
        delete cur_dir->path;
        delete cur_dir;
    }
    this->root = nullptr;
    this->directories.clear();

    /* Delete files. */
    for (const auto& it : this->files) {
        cur_file = it.second;
        delete cur_file->path;
        delete cur_file;
    }
    this->files.clear();

    /* Set header fields. */
    header->header_size = sizeof(*header);
    header->file_hash_table_size = this->file_hash_table_size;
    header->file_table_size = this->file_table_size;
    header->dir_hash_table_size = this->dir_hash_table_size;
    header->dir_table_size = this->dir_table_size;
    header->file_partition_ofs = ROMFS_FILEPARTITION_OFS;
    header->dir_hash_table_ofs =
        (header->file_partition_ofs + this->file_partition_size + 3ULL) & ~3ULL;
    header->dir_table_ofs = header->dir_hash_table_ofs + header->dir_hash_table_size;
    header->file_hash_table_ofs = header->dir_table_ofs + header->dir_table_size;
    header->file_table_ofs = header->file_hash_table_ofs + header->file_hash_table_size;

    std::vector<u8> header_data(sizeof(RomFSHeader));
    std::memcpy(header_data.data(), header, header_data.size());
    out.emplace(0, std::make_shared<VectorVfsFile>(header_data));

    std::vector<u8> meta_out(this->file_hash_table_size + this->file_table_size +
                             this->dir_hash_table_size + this->dir_table_size);
    std::memcpy(meta_out.data(), metadata, meta_out.size());
    out.emplace(header->dir_hash_table_ofs, std::make_shared<VectorVfsFile>(meta_out));

    return out;
}

} // namespace FileSys
