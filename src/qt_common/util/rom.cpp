// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_common/util/rom.h"

#include <QCoreApplication>

namespace QtCommon::ROM {

bool RomFSRawCopy(size_t total_size,
                  size_t& read_size,
                  QtProgressCallback callback,
                  const FileSys::VirtualDir& src,
                  const FileSys::VirtualDir& dest,
                  bool full)
{
    // TODO(crueter)
    // if (src == nullptr || dest == nullptr || !src->IsReadable() || !dest->IsWritable())
    //     return false;
    // if (dialog.wasCanceled())
    //     return false;

    // std::vector<u8> buffer(CopyBufferSize);
    // auto last_timestamp = std::chrono::steady_clock::now();

    // const auto QtRawCopy = [&](const FileSys::VirtualFile& src_file,
    //                            const FileSys::VirtualFile& dest_file) {
    //     if (src_file == nullptr || dest_file == nullptr) {
    //         return false;
    //     }
    //     if (!dest_file->Resize(src_file->GetSize())) {
    //         return false;
    //     }

    //     for (std::size_t i = 0; i < src_file->GetSize(); i += buffer.size()) {
    //         if (dialog.wasCanceled()) {
    //             dest_file->Resize(0);
    //             return false;
    //         }

    //         using namespace std::literals::chrono_literals;
    //         const auto new_timestamp = std::chrono::steady_clock::now();

    //         if ((new_timestamp - last_timestamp) > 33ms) {
    //             last_timestamp = new_timestamp;
    //             dialog.setValue(
    //                 static_cast<int>(std::min(read_size, total_size) * 100 / total_size));
    //             QCoreApplication::processEvents();
    //         }

    //         const auto read = src_file->Read(buffer.data(), buffer.size(), i);
    //         dest_file->Write(buffer.data(), read, i);

    //         read_size += read;
    //     }

    //     return true;
    // };

    // if (full) {
    //     for (const auto& file : src->GetFiles()) {
    //         const auto out = VfsDirectoryCreateFileWrapper(dest, file->GetName());
    //         if (!QtRawCopy(file, out))
    //             return false;
    //     }
    // }

    // for (const auto& dir : src->GetSubdirectories()) {
    //     const auto out = dest->CreateSubdirectory(dir->GetName());
    //     if (!RomFSRawCopy(total_size, read_size, dialog, dir, out, full))
    //         return false;
    // }

    // return true;
    return true;
}

}
