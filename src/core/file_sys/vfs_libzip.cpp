// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <zip.h>
#include "common/logging/backend.h"
#include "core/file_sys/vfs.h"
#include "core/file_sys/vfs_libzip.h"
#include "core/file_sys/vfs_vector.h"

namespace FileSys {

VirtualDir ExtractZIP(VirtualFile file) {
    zip_error_t error{};

    const auto data = file->ReadAllBytes();
    const auto src = zip_source_buffer_create(data.data(), data.size(), 0, &error);
    if (src == nullptr)
        return nullptr;

    const auto zip = zip_open_from_source(src, 0, &error);
    if (zip == nullptr)
        return nullptr;

    std::shared_ptr<VectorVfsDirectory> out = std::make_shared<VectorVfsDirectory>();

    const auto num_entries = zip_get_num_entries(zip, 0);
    if (num_entries == -1)
        return nullptr;

    zip_stat_t stat{};
    zip_stat_init(&stat);

    for (std::size_t i = 0; i < num_entries; ++i) {
        const auto stat_res = zip_stat_index(zip, i, 0, &stat);
        if (stat_res == -1)
            return nullptr;

        const std::string name(stat.name);
        if (name.empty())
            continue;

        if (name[name.size() - 1] != '/') {
            const auto file = zip_fopen_index(zip, i, 0);

            std::vector<u8> buf(stat.size);
            if (zip_fread(file, buf.data(), buf.size()) != buf.size())
                return nullptr;

            zip_fclose(file);

            const auto parts = FileUtil::SplitPathComponents(stat.name);
            const auto new_file = std::make_shared<VectorVfsFile>(buf, parts.back());

            std::shared_ptr<VectorVfsDirectory> dtrv = out;
            for (std::size_t j = 0; j < parts.size() - 1; ++j) {
                if (dtrv == nullptr)
                    return nullptr;
                const auto subdir = dtrv->GetSubdirectory(parts[j]);
                if (subdir == nullptr) {
                    const auto temp = std::make_shared<VectorVfsDirectory>(
                        std::vector<VirtualFile>{}, std::vector<VirtualDir>{}, parts[j]);
                    dtrv->AddDirectory(temp);
                    dtrv = temp;
                } else {
                    dtrv = std::dynamic_pointer_cast<VectorVfsDirectory>(subdir);
                }
            }

            if (dtrv == nullptr)
                return nullptr;
            dtrv->AddFile(new_file);
        }
    }

    zip_source_close(src);
    zip_close(zip);

    return out;
}

} // namespace FileSys
