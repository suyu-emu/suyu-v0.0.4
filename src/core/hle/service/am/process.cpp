// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"

#include "core/file_sys/content_archive.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/registered_cache.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/service/am/process.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/loader.h"

namespace Service::AM {

Process::Process(Core::System& system)
    : m_system(system), m_process(), m_main_thread_priority(), m_main_thread_stack_size(),
      m_program_id(), m_process_started() {}

Process::~Process() {
    this->Finalize();
}

bool Process::Initialize(u64 program_id, u8 minimum_key_generation, u8 maximum_key_generation) {
    // First, ensure we are not holding another process.
    this->Finalize();

    // Get the filesystem controller.
    auto& fsc = m_system.GetFileSystemController();

    // Attempt to load program NCA.
    const FileSys::RegisteredCache* bis_system{};
    FileSys::VirtualFile nca_raw{};

    // Get the program NCA from built-in storage.
    bis_system = fsc.GetSystemNANDContents();
    if (bis_system) {
        nca_raw = bis_system->GetEntryRaw(program_id, FileSys::ContentRecordType::Program);
    }

    // Ensure we retrieved a program NCA.
    if (!nca_raw) {
        return false;
    }

    // Ensure we have a suitable version.
    if (minimum_key_generation > 0) {
        FileSys::NCA nca(nca_raw);
        if (nca.GetStatus() == Loader::ResultStatus::Success &&
            (nca.GetKeyGeneration() < minimum_key_generation ||
             nca.GetKeyGeneration() > maximum_key_generation)) {
            LOG_WARNING(Service_LDR, "Skipping program {:016X} with generation {}", program_id,
                        nca.GetKeyGeneration());
            return false;
        }
    }

    // Get the appropriate loader to parse this NCA.
    auto app_loader = Loader::GetLoader(m_system, nca_raw, program_id, 0);

    // Ensure we have a loader which can parse the NCA.
    if (!app_loader) {
        return false;
    }

    // Create the process.
    auto* const process = Kernel::KProcess::Create(m_system.Kernel());
    Kernel::KProcess::Register(m_system.Kernel(), process);

    // On exit, ensure we free the additional reference to the process.
    SCOPE_EXIT {
        process->Close();
    };

    // Insert process modules into memory.
    const auto [load_result, load_parameters] = app_loader->Load(*process, m_system);

    // Ensure loading was successful.
    if (load_result != Loader::ResultStatus::Success) {
        return false;
    }

    // TODO: remove this, kernel already tracks this
    m_system.Kernel().AppendNewProcess(process);

    // Note the load parameters from NPDM.
    m_main_thread_priority = load_parameters->main_thread_priority;
    m_main_thread_stack_size = load_parameters->main_thread_stack_size;

    // This process has not started yet.
    m_process_started = false;

    // Take ownership of the process object.
    m_process = process;
    m_process->Open();

    // We succeeded.
    return true;
}

void Process::Finalize() {
    // Terminate, if we are currently holding a process.
    this->Terminate();

    // Close the process.
    if (m_process) {
        m_process->Close();

        // TODO: remove this, kernel already tracks this
        m_system.Kernel().RemoveProcess(m_process);
    }

    // Clean up.
    m_process = nullptr;
    m_main_thread_priority = 0;
    m_main_thread_stack_size = 0;
    m_program_id = 0;
    m_process_started = false;
}

bool Process::Run() {
    // If we already started the process, don't start again.
    if (m_process_started) {
        return false;
    }

    // Start.
    if (m_process) {
        m_process->Run(m_main_thread_priority, m_main_thread_stack_size);
    }

    // Mark as started.
    m_process_started = true;

    // We succeeded.
    return true;
}

void Process::Terminate() {
    if (m_process) {
        m_process->Terminate();
    }
}

u64 Process::GetProcessId() const {
    if (m_process) {
        return m_process->GetProcessId();
    }

    return 0;
}

} // namespace Service::AM
