// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "common/common_types.h"
#include "core/loader/loader.h"

namespace Core {
class System;
}

namespace FileSys {
class NACP;
}

namespace Kernel {
class Process;
}

namespace Loader {

/// Loads an NRO file
class AppLoader_NRO final : public AppLoader {
public:
    explicit AppLoader_NRO(FileSys::VirtualFile file);
    ~AppLoader_NRO() override;

    /**
     * Returns the type of the file
     * @param file open file
     * @return FileType found, or FileType::Error if this loader doesn't know it
     */
    static FileType IdentifyType(const FileSys::VirtualFile& file);

    FileType GetFileType() const override {
        return IdentifyType(file);
    }

    LoadResult Load(Kernel::Process& process, Core::System& system) override;

    ResultStatus ReadIcon(std::vector<u8>& buffer) override;
    ResultStatus ReadProgramId(u64& out_program_id) override;
    ResultStatus ReadRomFS(FileSys::VirtualFile& dir) override;
    ResultStatus ReadTitle(std::string& title) override;
    ResultStatus ReadControlData(FileSys::NACP& control) override;
    bool IsRomFSUpdatable() const override;

private:
    bool LoadNro(Kernel::Process& process, const FileSys::VfsFile& file);

    std::vector<u8> icon_data;
    std::unique_ptr<FileSys::NACP> nacp;
    FileSys::VirtualFile romfs;
};

} // namespace Loader
