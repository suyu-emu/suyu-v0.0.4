// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/service.h"

namespace Service::NS {

class PL_U final : public ServiceFramework<PL_U> {
public:
    PL_U();
    ~PL_U() override;

private:
    void RequestLoad(Kernel::HLERequestContext& ctx);
    void GetLoadState(Kernel::HLERequestContext& ctx);
    void GetSize(Kernel::HLERequestContext& ctx);
    void GetSharedMemoryAddressOffset(Kernel::HLERequestContext& ctx);
    void GetSharedMemoryNativeHandle(Kernel::HLERequestContext& ctx);
    void GetSharedFontInOrderOfPriority(Kernel::HLERequestContext& ctx);

    /// Handle to shared memory region designated for a shared font
    Kernel::SharedPtr<Kernel::SharedMemory> shared_font_mem;

    /// Backing memory for the shared font data
    std::shared_ptr<std::vector<u8>> shared_font;
};

} // namespace Service::NS
