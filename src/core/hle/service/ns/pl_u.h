// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/service/service.h"

namespace Service {

namespace FileSystem {
class FileSystemController;
} // namespace FileSystem

namespace NS {

class PL_U final : public ServiceFramework<PL_U> {
public:
    PL_U(FileSystem::FileSystemController& fsc);
    ~PL_U() override;

private:
    void RequestLoad(Kernel::HLERequestContext& ctx);
    void GetLoadState(Kernel::HLERequestContext& ctx);
    void GetSize(Kernel::HLERequestContext& ctx);
    void GetSharedMemoryAddressOffset(Kernel::HLERequestContext& ctx);
    void GetSharedMemoryNativeHandle(Kernel::HLERequestContext& ctx);
    void GetSharedFontInOrderOfPriority(Kernel::HLERequestContext& ctx);

    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace NS

} // namespace Service
