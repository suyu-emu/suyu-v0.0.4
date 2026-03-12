// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/logging.h"
#include "video_core/engines/engine_interface.h"
#include "video_core/engines/engine_upload.h"

namespace Core {
class System;
}

namespace Tegra {
class MemoryManager;
}

namespace Tegra::Engines {
class Nv01Timer final : public EngineInterface {
public:
    explicit Nv01Timer(Core::System& system_, MemoryManager& memory_manager)
        : system{system_}
    {}
    ~Nv01Timer() override;

    /// Write the value to the register identified by method.
    void CallMethod(u32 method, u32 method_argument, bool is_last_call) override {
        LOG_DEBUG(HW_GPU, "method={}, argument={}, is_last_call={}", method, method_argument, is_last_call);
    }

    /// Write multiple values to the register identified by method.
    void CallMultiMethod(u32 method, const u32* base_start, u32 amount, u32 methods_pending) override {
        LOG_DEBUG(HW_GPU, "method={}, base_start={}, amount={}, pending={}", method, fmt::ptr(base_start), amount, methods_pending);
    }

    struct Regs {
        // No fucking idea
        INSERT_PADDING_BYTES_NOINIT(0x48);
    } regs{};
private:
    void ConsumeSinkImpl() override {}
    Core::System& system;
};
}
