// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"
#include "core/hle/result.h"
#include "hid_core/hid_types.h"

namespace Kernel {
class KernelCore;
}

namespace Service::HID {
class NpadAbstractedPadHolder;
class NpadAbstractPropertiesHandler;
class PalmaResource;

class NpadAbstractPalmaHandler final {
public:
    explicit NpadAbstractPalmaHandler(Kernel::KernelCore& kernel_);
    ~NpadAbstractPalmaHandler();

    void SetAbstractPadHolder(NpadAbstractedPadHolder* holder);
    void SetPropertiesHandler(NpadAbstractPropertiesHandler* handler);
    void SetPalmaResource(PalmaResource* resource);

    Result IncrementRefCounter();
    Result DecrementRefCounter();

    void UpdatePalmaState();

private:
    NpadAbstractedPadHolder* abstract_pad_holder{nullptr};
    NpadAbstractPropertiesHandler* properties_handler{nullptr};
    PalmaResource* palma_resource{nullptr};

    s32 ref_counter{};
    Kernel::KernelCore& kernel;
};

} // namespace Service::HID
