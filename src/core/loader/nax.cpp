// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/xts_archive.h"
#include "core/hle/kernel/process.h"
#include "core/loader/nax.h"
#include "core/loader/nca.h"

namespace Loader {
namespace {
FileType IdentifyTypeImpl(const FileSys::NAX& nax) {
    if (nax.GetStatus() != ResultStatus::Success) {
        return FileType::Error;
    }

    const auto nca = nax.AsNCA();
    if (nca == nullptr || nca->GetStatus() != ResultStatus::Success) {
        return FileType::Error;
    }

    return FileType::NAX;
}
} // Anonymous namespace

AppLoader_NAX::AppLoader_NAX(FileSys::VirtualFile file)
    : AppLoader(file), nax(std::make_unique<FileSys::NAX>(file)),
      nca_loader(std::make_unique<AppLoader_NCA>(nax->GetDecrypted())) {}

AppLoader_NAX::~AppLoader_NAX() = default;

FileType AppLoader_NAX::IdentifyType(const FileSys::VirtualFile& file) {
    const FileSys::NAX nax(file);
    return IdentifyTypeImpl(nax);
}

FileType AppLoader_NAX::GetFileType() {
    return IdentifyTypeImpl(*nax);
}

ResultStatus AppLoader_NAX::Load(Kernel::SharedPtr<Kernel::Process>& process) {
    if (is_loaded) {
        return ResultStatus::ErrorAlreadyLoaded;
    }

    if (nax->GetStatus() != ResultStatus::Success)
        return nax->GetStatus();

    const auto nca = nax->AsNCA();
    if (nca == nullptr) {
        if (!Core::Crypto::KeyManager::KeyFileExists(false))
            return ResultStatus::ErrorMissingProductionKeyFile;
        return ResultStatus::ErrorNAXInconvertibleToNCA;
    }

    if (nca->GetStatus() != ResultStatus::Success)
        return nca->GetStatus();

    const auto result = nca_loader->Load(process);
    if (result != ResultStatus::Success)
        return result;

    is_loaded = true;

    return ResultStatus::Success;
}

ResultStatus AppLoader_NAX::ReadRomFS(FileSys::VirtualFile& dir) {
    return nca_loader->ReadRomFS(dir);
}

ResultStatus AppLoader_NAX::ReadProgramId(u64& out_program_id) {
    return nca_loader->ReadProgramId(out_program_id);
}
} // namespace Loader
