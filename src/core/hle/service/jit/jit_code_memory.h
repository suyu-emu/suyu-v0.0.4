// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <random>

#include "core/hle/kernel/k_code_memory.h"

namespace Kernel {
class KernelCore;
}

namespace Service::JIT {

class CodeMemory {
public:
    Result Initialize(Kernel::KernelCore& kernel, Kernel::KProcess& process, Kernel::KCodeMemory& code_memory, size_t size, Kernel::Svc::MemoryPermission perm, std::mt19937_64& generate_random);
    void Finalize(Kernel::KernelCore& kernel);

    size_t GetSize() const {
        return m_size;
    }

    u64 GetAddress() const {
        return m_address;
    }

    Kernel::KCodeMemory* m_code_memory{};
    size_t m_size{};
    u64 m_address{};
    Kernel::Svc::MemoryPermission m_perm{};
};

} // namespace Service::JIT
