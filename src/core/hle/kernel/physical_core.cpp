// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <atomic>
#include <chrono>
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/debugger/debugger.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/kernel/svc.h"
#include "core/memory.h"

namespace Kernel {

PhysicalCore::PhysicalCore(KernelCore& kernel, std::size_t core_index)
    : m_kernel{kernel}, m_core_index{core_index} {
    m_is_single_core = !kernel.IsMulticore();
}
PhysicalCore::~PhysicalCore() = default;

void PhysicalCore::RunThread(Kernel::KThread* thread) {
    auto* process = thread->GetOwnerProcess();
    auto& system = m_kernel.System();
    auto* interface = process->GetArmInterface(m_core_index);

    interface->Initialize();

    const auto EnterContext = [&]() {
        system.EnterCPUProfile();

        // Lock the core context.
        std::scoped_lock lk{m_guard};

        // Check if we are already interrupted. If we are, we can just stop immediately.
        if (m_is_interrupted) {
            return false;
        }

        // Mark that we are running.
        m_arm_interface = interface;
        m_current_thread = thread;

        // Acquire the lock on the thread parameters.
        // This allows us to force synchronization with Interrupt.
        interface->LockThread(thread);

        return true;
    };

    const auto ExitContext = [&]() {
        // Unlock the thread.
        interface->UnlockThread(thread);

        // Lock the core context.
        std::scoped_lock lk{m_guard};

        // On exit, we no longer are running.
        m_arm_interface = nullptr;
        m_current_thread = nullptr;

        system.ExitCPUProfile();
    };

    while (true) {
        // If the thread is scheduled for termination, exit.
        if (thread->HasDpc() && thread->IsTerminationRequested()) {
            thread->Exit();
        }

        // Notify the debugger and go to sleep if a step was performed
        // and this thread has been scheduled again.
        if (thread->GetStepState() == StepState::StepPerformed) {
            system.GetDebugger().NotifyThreadStopped(thread);
            thread->RequestSuspend(SuspendType::Debug);
            return;
        }

        // Otherwise, run the thread.
        Core::HaltReason hr{};
        {
            // If we were interrupted, exit immediately.
            if (!EnterContext()) {
                return;
            }

            if (thread->GetStepState() == StepState::StepPending) {
                hr = interface->StepThread(thread);

                if (True(hr & Core::HaltReason::StepThread)) {
                    thread->SetStepState(StepState::StepPerformed);
                }
            } else {
                hr = interface->RunThread(thread);
            }

            ExitContext();
        }

        // Determine why we stopped.
        const bool supervisor_call = True(hr & Core::HaltReason::SupervisorCall);
        const bool prefetch_abort = True(hr & Core::HaltReason::PrefetchAbort);
        const bool breakpoint = True(hr & Core::HaltReason::InstructionBreakpoint);
        const bool data_abort = True(hr & Core::HaltReason::DataAbort);
        const bool interrupt = True(hr & Core::HaltReason::BreakLoop);

        // Since scheduling may occur here, we cannot use any cached
        // state after returning from calls we make.

        // Notify the debugger and go to sleep if a breakpoint was hit,
        // or if the thread is unable to continue for any reason.
        if (breakpoint || prefetch_abort) {
            if (breakpoint) {
                interface->RewindBreakpointInstruction();
            }
            if (system.DebuggerEnabled()) {
                system.GetDebugger().NotifyThreadStopped(thread);
            } else {
                interface->LogBacktrace(process);
            }
            thread->RequestSuspend(SuspendType::Debug);
            return;
        }

        // Notify the debugger and go to sleep on data abort.
        if (data_abort) {
            if (system.DebuggerEnabled()) {
                system.GetDebugger().NotifyThreadWatchpoint(thread, *interface->HaltedWatchpoint());
            }
            thread->RequestSuspend(SuspendType::Debug);
            return;
        }

        // Handle system calls.
        if (supervisor_call) {
            Svc::Call(system, interface->GetSvcNumber());
            return;
        }

        // TEMPORARY spin diagnostic (Tomodachi Life busy-loop): every ~15s per core, log
        // the running thread's PC and the instruction words around it so a guest thread
        // that spins in pure JIT (no SVCs) can be identified. Remove once diagnosed.
        {
            static thread_local std::chrono::steady_clock::time_point last_beat{};
            const auto now = std::chrono::steady_clock::now();
            if (now - last_beat > std::chrono::seconds(15)) {
                last_beat = now;
                Kernel::Svc::ThreadContext ctx{};
                interface->GetContext(ctx);
                auto& memory = process->GetMemory();
                std::array<u32, 8> insns{};
                for (size_t i = 0; i < insns.size(); ++i) {
                    insns[i] = memory.Read32(ctx.pc + i * 4);
                }
                // Scan the top of the stack for plausible return addresses (code region)
                // to reconstruct an approximate call chain without symbols.
                std::array<u64, 6> ret_addrs{};
                size_t found = 0;
                for (u64 off = 0; off < 0x600 && found < ret_addrs.size(); off += 8) {
                    const u64 v = memory.Read64(ctx.sp + off);
                    if (v >= 0x8000'0000ULL && v < 0x1'0000'0000ULL && (v & 3) == 0) {
                        ret_addrs[found++] = v;
                    }
                }
                LOG_INFO(Kernel,
                         "SPINDIAG core={} tid={} pc={:#x} lr={:#x} sp={:#x} x0={:#x} x1={:#x} "
                         "x2={:#x} x19={:#x} insns=[{:08x} {:08x} {:08x} {:08x} {:08x} {:08x} "
                         "{:08x} {:08x}] callchain=[{:#x} {:#x} {:#x} {:#x} {:#x} {:#x}]",
                         m_core_index, thread->GetThreadId(), ctx.pc, ctx.lr, ctx.sp, ctx.r[0],
                         ctx.r[1], ctx.r[2], ctx.r[19], insns[0], insns[1], insns[2], insns[3],
                         insns[4], insns[5], insns[6], insns[7], ret_addrs[0], ret_addrs[1],
                         ret_addrs[2], ret_addrs[3], ret_addrs[4], ret_addrs[5]);

                // One-shot code dump of the stable Tomodachi dispatcher frames so the
                // outer loop's structure and exit condition can be decoded offline.
                static std::atomic_bool dumped{false};
                bool expected = false;
                if (dumped.compare_exchange_strong(expected, true)) {
                    constexpr std::array<u64, 3> bases{0x8000fa80ULL, 0x8082c180ULL,
                                                       0x821d3e00ULL};
                    for (const u64 base : bases) {
                        for (u64 row = 0; row < 8; ++row) {
                            const u64 addr = base + row * 32;
                            std::array<u32, 8> w{};
                            for (size_t i = 0; i < w.size(); ++i) {
                                w[i] = memory.Read32(addr + i * 4);
                            }
                            LOG_INFO(Kernel,
                                     "CODEDUMP {:#010x}: {:08x} {:08x} {:08x} {:08x} {:08x} "
                                     "{:08x} {:08x} {:08x}",
                                     addr, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
                        }
                    }
                }
            }
        }

        // Handle external interrupt sources.
        if (interrupt || m_is_single_core) {
            return;
        }
    }
}

void PhysicalCore::LoadContext(const KThread* thread) {
    auto* const process = thread->GetOwnerProcess();
    if (!process) {
        // Kernel threads do not run on emulated CPU cores.
        return;
    }

    auto* interface = process->GetArmInterface(m_core_index);
    if (interface) {
        interface->SetContext(thread->GetContext());
        interface->SetTpidrroEl0(GetInteger(thread->GetTlsAddress()));
        interface->SetWatchpointArray(&process->GetWatchpoints());
    }
}

void PhysicalCore::LoadSvcArguments(const KProcess& process, std::span<const uint64_t, 8> args) {
    process.GetArmInterface(m_core_index)->SetSvcArguments(args);
}

void PhysicalCore::SaveContext(KThread* thread) const {
    auto* const process = thread->GetOwnerProcess();
    if (!process) {
        // Kernel threads do not run on emulated CPU cores.
        return;
    }

    auto* interface = process->GetArmInterface(m_core_index);
    if (interface) {
        interface->GetContext(thread->GetContext());
    }
}

void PhysicalCore::SaveSvcArguments(KProcess& process, std::span<uint64_t, 8> args) const {
    process.GetArmInterface(m_core_index)->GetSvcArguments(args);
}

void PhysicalCore::CloneFpuStatus(KThread* dst) const {
    auto* process = dst->GetOwnerProcess();

    Svc::ThreadContext ctx{};
    process->GetArmInterface(m_core_index)->GetContext(ctx);

    dst->GetContext().fpcr = ctx.fpcr;
    dst->GetContext().fpsr = ctx.fpsr;
}

void PhysicalCore::LogBacktrace() {
    auto* process = GetCurrentProcessPointer(m_kernel);
    if (!process) {
        return;
    }

    auto* interface = process->GetArmInterface(m_core_index);
    if (interface) {
        interface->LogBacktrace(process);
    }
}

void PhysicalCore::Idle() {
    std::unique_lock lk{m_guard};
    m_on_interrupt.wait(lk, [this] { return m_is_interrupted; });
}

bool PhysicalCore::IsInterrupted() const {
    return m_is_interrupted;
}

void PhysicalCore::Interrupt() {
    // Lock core context.
    std::scoped_lock lk{m_guard};

    // Load members.
    auto* arm_interface = m_arm_interface;
    auto* thread = m_current_thread;

    // Add interrupt flag.
    m_is_interrupted = true;

    // Interrupt ourselves.
    m_on_interrupt.notify_one();

    // If there is no thread running, we are done.
    if (arm_interface == nullptr) {
        return;
    }

    // Interrupt the CPU.
    arm_interface->SignalInterrupt(thread);
}

void PhysicalCore::ClearInterrupt() {
    std::scoped_lock lk{m_guard};
    m_is_interrupted = false;
}

} // namespace Kernel
