// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "core/hle/service/service.h"

namespace Service {

namespace FileSystem {
class FileSystemController;
} // namespace FileSystem

namespace NS {

void EncryptSharedFont(const std::vector<u32>& input, std::vector<u8>& output, std::size_t& offset);

class PL_U final : public ServiceFramework<PL_U> {
public:
    explicit PL_U(Core::System& system);
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
    Core::System& system;
};

} // namespace NS

} // namespace Service
