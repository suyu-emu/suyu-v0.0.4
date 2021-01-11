// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/loader/loader.h"

namespace Core {
class System;
}

namespace FileSys {
class KIP;
}

namespace Loader {

class AppLoader_KIP final : public AppLoader {
public:
    explicit AppLoader_KIP(FileSys::VirtualFile file);
    ~AppLoader_KIP() override;

    /**
     * Returns the type of the file
     * @param file open file
     * @return FileType found, or FileType::Error if this loader doesn't know it
     */
    static FileType IdentifyType(const FileSys::VirtualFile& file);

    FileType GetFileType() const override;

    LoadResult Load(Kernel::Process& process, Core::System& system) override;

private:
    std::unique_ptr<FileSys::KIP> kip;
};

} // namespace Loader
