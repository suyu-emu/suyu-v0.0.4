// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/fs_filesystem.h"
#include "core/file_sys/savedata_factory.h"
#include "core/hle/service/filesystem/fsp/fs_i_directory.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::FileSystem {

template <typename T>
static void BuildEntryIndex(std::vector<FileSys::DirectoryEntry>& entries,
                            const std::vector<T>& new_data, FileSys::DirectoryEntryType type) {
    entries.reserve(entries.size() + new_data.size());

    for (const auto& new_entry : new_data) {
        auto name = new_entry->GetName();

        if (type == FileSys::DirectoryEntryType::File &&
            name == FileSys::GetSaveDataSizeFileName()) {
            continue;
        }

        entries.emplace_back(name, static_cast<s8>(type),
                             type == FileSys::DirectoryEntryType::Directory ? 0
                                                                            : new_entry->GetSize());
    }
}

IDirectory::IDirectory(Core::System& system_, FileSys::VirtualDir backend_,
                       FileSys::OpenDirectoryMode mode)
    : ServiceFramework{system_, "IDirectory"}, backend(std::move(backend_)) {
    static const FunctionInfo functions[] = {
        {0, &IDirectory::Read, "Read"},
        {1, &IDirectory::GetEntryCount, "GetEntryCount"},
    };
    RegisterHandlers(functions);

    // TODO(DarkLordZach): Verify that this is the correct behavior.
    // Build entry index now to save time later.
    if (True(mode & FileSys::OpenDirectoryMode::Directory)) {
        BuildEntryIndex(entries, backend->GetSubdirectories(),
                        FileSys::DirectoryEntryType::Directory);
    }
    if (True(mode & FileSys::OpenDirectoryMode::File)) {
        BuildEntryIndex(entries, backend->GetFiles(), FileSys::DirectoryEntryType::File);
    }
}

void IDirectory::Read(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "called.");

    // Calculate how many entries we can fit in the output buffer
    const u64 count_entries = ctx.GetWriteBufferNumElements<FileSys::DirectoryEntry>();

    // Cap at total number of entries.
    const u64 actual_entries = std::min(count_entries, entries.size() - next_entry_index);

    // Determine data start and end
    const auto* begin = reinterpret_cast<u8*>(entries.data() + next_entry_index);
    const auto* end = reinterpret_cast<u8*>(entries.data() + next_entry_index + actual_entries);
    const auto range_size = static_cast<std::size_t>(std::distance(begin, end));

    next_entry_index += actual_entries;

    // Write the data to memory
    ctx.WriteBuffer(begin, range_size);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(actual_entries);
}

void IDirectory::GetEntryCount(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "called");

    u64 count = entries.size() - next_entry_index;

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(count);
}

} // namespace Service::FileSystem
