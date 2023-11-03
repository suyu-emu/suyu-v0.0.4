// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "common/common_types.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/file_sys/fsmitm_romfsbuild.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/vfs.h"
#include "core/file_sys/vfs_cached.h"
#include "core/file_sys/vfs_concat.h"
#include "core/file_sys/vfs_offset.h"
#include "core/file_sys/vfs_vector.h"

namespace FileSys {
namespace {
constexpr u32 ROMFS_ENTRY_EMPTY = 0xFFFFFFFF;

struct TableLocation {
    u64_le offset;
    u64_le size;
};
static_assert(sizeof(TableLocation) == 0x10, "TableLocation has incorrect size.");

struct RomFSHeader {
    u64_le header_size;
    TableLocation directory_hash;
    TableLocation directory_meta;
    TableLocation file_hash;
    TableLocation file_meta;
    u64_le data_offset;
};
static_assert(sizeof(RomFSHeader) == 0x50, "RomFSHeader has incorrect size.");

struct DirectoryEntry {
    u32_le parent;
    u32_le sibling;
    u32_le child_dir;
    u32_le child_file;
    u32_le hash;
    u32_le name_length;
};
static_assert(sizeof(DirectoryEntry) == 0x18, "DirectoryEntry has incorrect size.");

struct FileEntry {
    u32_le parent;
    u32_le sibling;
    u64_le offset;
    u64_le size;
    u32_le hash;
    u32_le name_length;
};
static_assert(sizeof(FileEntry) == 0x20, "FileEntry has incorrect size.");

template <typename Entry>
std::pair<Entry, std::string> GetEntry(const VirtualFile& file, std::size_t offset) {
    Entry entry{};
    if (file->ReadObject(&entry, offset) != sizeof(Entry))
        return {};
    std::string string(entry.name_length, '\0');
    if (file->ReadArray(&string[0], string.size(), offset + sizeof(Entry)) != string.size())
        return {};
    return {entry, string};
}

void ProcessFile(const VirtualFile& file, std::size_t file_offset, std::size_t data_offset,
                 u32 this_file_offset, std::shared_ptr<VectorVfsDirectory>& parent) {
    while (this_file_offset != ROMFS_ENTRY_EMPTY) {
        auto entry = GetEntry<FileEntry>(file, file_offset + this_file_offset);

        parent->AddFile(std::make_shared<OffsetVfsFile>(
            file, entry.first.size, entry.first.offset + data_offset, entry.second));

        this_file_offset = entry.first.sibling;
    }
}

void ProcessDirectory(const VirtualFile& file, std::size_t dir_offset, std::size_t file_offset,
                      std::size_t data_offset, u32 this_dir_offset,
                      std::shared_ptr<VectorVfsDirectory>& parent) {
    while (this_dir_offset != ROMFS_ENTRY_EMPTY) {
        auto entry = GetEntry<DirectoryEntry>(file, dir_offset + this_dir_offset);
        auto current = std::make_shared<VectorVfsDirectory>(
            std::vector<VirtualFile>{}, std::vector<VirtualDir>{}, entry.second);

        if (entry.first.child_file != ROMFS_ENTRY_EMPTY) {
            ProcessFile(file, file_offset, data_offset, entry.first.child_file, current);
        }

        if (entry.first.child_dir != ROMFS_ENTRY_EMPTY) {
            ProcessDirectory(file, dir_offset, file_offset, data_offset, entry.first.child_dir,
                             current);
        }

        parent->AddDirectory(current);
        this_dir_offset = entry.first.sibling;
    }
}
} // Anonymous namespace

VirtualDir ExtractRomFS(VirtualFile file) {
    RomFSHeader header{};
    if (file->ReadObject(&header) != sizeof(RomFSHeader))
        return nullptr;

    if (header.header_size != sizeof(RomFSHeader))
        return nullptr;

    const u64 file_offset = header.file_meta.offset;
    const u64 dir_offset = header.directory_meta.offset;

    auto root_container = std::make_shared<VectorVfsDirectory>();

    ProcessDirectory(file, dir_offset, file_offset, header.data_offset, 0, root_container);

    if (auto root = root_container->GetSubdirectory(""); root) {
        return std::make_shared<CachedVfsDirectory>(std::move(root));
    }

    return nullptr;
}

VirtualFile CreateRomFS(VirtualDir dir, VirtualDir ext) {
    if (dir == nullptr)
        return nullptr;

    RomFSBuildContext ctx{dir, ext};
    return ConcatenatedVfsFile::MakeConcatenatedFile(0, dir->GetName(), ctx.Build());
}

} // namespace FileSys
