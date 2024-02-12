// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Kernel {
class KProcess;
}

namespace Core {
class System;
}

namespace Service::AM {

class Process {
public:
    explicit Process(Core::System& system);
    ~Process();

    bool Initialize(u64 program_id, u8 minimum_key_generation, u8 maximum_key_generation);
    void Finalize();

    bool Run();
    void Terminate();

    bool IsInitialized() const {
        return m_process != nullptr;
    }
    u64 GetProcessId() const;
    u64 GetProgramId() const {
        return m_program_id;
    }
    Kernel::KProcess* GetProcess() const {
        return m_process;
    }

private:
    Core::System& m_system;
    Kernel::KProcess* m_process{};
    s32 m_main_thread_priority{};
    u64 m_main_thread_stack_size{};
    u64 m_program_id{};
    bool m_process_started{};
};

} // namespace Service::AM
