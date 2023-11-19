// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cinttypes>
#include <memory>

#include "common/signal_chain.h"
#include "core/arm/nce/arm_nce.h"
#include "core/arm/nce/patch.h"
#include "core/core.h"
#include "core/memory.h"

#include "core/hle/kernel/k_process.h"

#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace Core {

namespace {

struct sigaction g_orig_action;

// Verify assembly offsets.
using NativeExecutionParameters = Kernel::KThread::NativeExecutionParameters;
static_assert(offsetof(NativeExecutionParameters, native_context) == TpidrEl0NativeContext);
static_assert(offsetof(NativeExecutionParameters, lock) == TpidrEl0Lock);
static_assert(offsetof(NativeExecutionParameters, magic) == TpidrEl0TlsMagic);

fpsimd_context* GetFloatingPointState(mcontext_t& host_ctx) {
    _aarch64_ctx* header = reinterpret_cast<_aarch64_ctx*>(&host_ctx.__reserved);
    while (header->magic != FPSIMD_MAGIC) {
        header = reinterpret_cast<_aarch64_ctx*>(reinterpret_cast<char*>(header) + header->size);
    }
    return reinterpret_cast<fpsimd_context*>(header);
}

} // namespace

void* ARM_NCE::RestoreGuestContext(void* raw_context) {
    // Retrieve the host context.
    auto& host_ctx = static_cast<ucontext_t*>(raw_context)->uc_mcontext;

    // Thread-local parameters will be located in x9.
    auto* tpidr = reinterpret_cast<NativeExecutionParameters*>(host_ctx.regs[9]);
    auto* guest_ctx = static_cast<GuestContext*>(tpidr->native_context);

    // Retrieve the host floating point state.
    auto* fpctx = GetFloatingPointState(host_ctx);

    // Save host callee-saved registers.
    std::memcpy(guest_ctx->host_ctx.host_saved_vregs.data(), &fpctx->vregs[8],
                sizeof(guest_ctx->host_ctx.host_saved_vregs));
    std::memcpy(guest_ctx->host_ctx.host_saved_regs.data(), &host_ctx.regs[19],
                sizeof(guest_ctx->host_ctx.host_saved_regs));

    // Save stack pointer.
    guest_ctx->host_ctx.host_sp = host_ctx.sp;

    // Restore all guest state except tpidr_el0.
    host_ctx.sp = guest_ctx->sp;
    host_ctx.pc = guest_ctx->pc;
    host_ctx.pstate = guest_ctx->pstate;
    fpctx->fpcr = guest_ctx->fpcr;
    fpctx->fpsr = guest_ctx->fpsr;
    std::memcpy(host_ctx.regs, guest_ctx->cpu_registers.data(), sizeof(host_ctx.regs));
    std::memcpy(fpctx->vregs, guest_ctx->vector_registers.data(), sizeof(fpctx->vregs));

    // Return the new thread-local storage pointer.
    return tpidr;
}

void ARM_NCE::SaveGuestContext(GuestContext* guest_ctx, void* raw_context) {
    // Retrieve the host context.
    auto& host_ctx = static_cast<ucontext_t*>(raw_context)->uc_mcontext;

    // Retrieve the host floating point state.
    auto* fpctx = GetFloatingPointState(host_ctx);

    // Save all guest registers except tpidr_el0.
    std::memcpy(guest_ctx->cpu_registers.data(), host_ctx.regs, sizeof(host_ctx.regs));
    std::memcpy(guest_ctx->vector_registers.data(), fpctx->vregs, sizeof(fpctx->vregs));
    guest_ctx->fpsr = fpctx->fpsr;
    guest_ctx->fpcr = fpctx->fpcr;
    guest_ctx->pstate = static_cast<u32>(host_ctx.pstate);
    guest_ctx->pc = host_ctx.pc;
    guest_ctx->sp = host_ctx.sp;

    // Restore stack pointer.
    host_ctx.sp = guest_ctx->host_ctx.host_sp;

    // Restore host callee-saved registers.
    std::memcpy(&host_ctx.regs[19], guest_ctx->host_ctx.host_saved_regs.data(),
                sizeof(guest_ctx->host_ctx.host_saved_regs));
    std::memcpy(&fpctx->vregs[8], guest_ctx->host_ctx.host_saved_vregs.data(),
                sizeof(guest_ctx->host_ctx.host_saved_vregs));

    // Return from the call on exit by setting pc to x30.
    host_ctx.pc = guest_ctx->host_ctx.host_saved_regs[11];

    // Clear esr_el1 and return it.
    host_ctx.regs[0] = guest_ctx->esr_el1.exchange(0);
}

bool ARM_NCE::HandleGuestFault(GuestContext* guest_ctx, void* raw_info, void* raw_context) {
    auto& host_ctx = static_cast<ucontext_t*>(raw_context)->uc_mcontext;
    auto* info = static_cast<siginfo_t*>(raw_info);

    // Try to handle an invalid access.
    // TODO: handle accesses which split a page?
    const Common::ProcessAddress addr =
        (reinterpret_cast<u64>(info->si_addr) & ~Memory::YUZU_PAGEMASK);
    if (guest_ctx->system->ApplicationMemory().InvalidateNCE(addr, Memory::YUZU_PAGESIZE)) {
        // We handled the access successfully and are returning to guest code.
        return true;
    }

    // We can't handle the access, so trigger an exception.
    const bool is_prefetch_abort = host_ctx.pc == reinterpret_cast<u64>(info->si_addr);
    guest_ctx->esr_el1.fetch_or(
        static_cast<u64>(is_prefetch_abort ? HaltReason::PrefetchAbort : HaltReason::DataAbort));

    // Forcibly mark the context as locked. We are still running.
    // We may race with SignalInterrupt here:
    // - If we lose the race, then SignalInterrupt will send us a signal we are masking,
    //   and it will do nothing when it is unmasked, as we have already left guest code.
    // - If we win the race, then SignalInterrupt will wait for us to unlock first.
    auto& thread_params = guest_ctx->parent->running_thread->GetNativeExecutionParameters();
    thread_params.lock.store(SpinLockLocked);

    // Return to host.
    SaveGuestContext(guest_ctx, raw_context);
    return false;
}

void ARM_NCE::HandleHostFault(int sig, void* raw_info, void* raw_context) {
    return g_orig_action.sa_sigaction(sig, static_cast<siginfo_t*>(raw_info), raw_context);
}

HaltReason ARM_NCE::RunJit() {
    // Get the thread parameters.
    // TODO: pass the current thread down from ::Run
    auto* thread = Kernel::GetCurrentThreadPointer(system.Kernel());
    auto* thread_params = &thread->GetNativeExecutionParameters();

    {
        // Lock our core context.
        std::scoped_lock lk{lock};

        // We should not be running.
        ASSERT(running_thread == nullptr);

        // Check if we need to run. If we have already been halted, we are done.
        u64 halt = guest_ctx.esr_el1.exchange(0);
        if (halt != 0) {
            return static_cast<HaltReason>(halt);
        }

        // Mark that we are running.
        running_thread = thread;

        // Acquire the lock on the thread parameters.
        // This allows us to force synchronization with SignalInterrupt.
        LockThreadParameters(thread_params);
    }

    // Assign current members.
    guest_ctx.parent = this;
    thread_params->native_context = &guest_ctx;
    thread_params->tpidr_el0 = guest_ctx.tpidr_el0;
    thread_params->tpidrro_el0 = guest_ctx.tpidrro_el0;
    thread_params->is_running = true;

    HaltReason halt{};

    // TODO: finding and creating the post handler needs to be locked
    // to deal with dynamic loading of NROs.
    const auto& post_handlers = system.ApplicationProcess()->GetPostHandlers();
    if (auto it = post_handlers.find(guest_ctx.pc); it != post_handlers.end()) {
        halt = ReturnToRunCodeByTrampoline(thread_params, &guest_ctx, it->second);
    } else {
        halt = ReturnToRunCodeByExceptionLevelChange(thread_id, thread_params);
    }

    // Unload members.
    // The thread does not change, so we can persist the old reference.
    guest_ctx.tpidr_el0 = thread_params->tpidr_el0;
    thread_params->native_context = nullptr;
    thread_params->is_running = false;

    // Unlock the thread parameters.
    UnlockThreadParameters(thread_params);

    {
        // Lock the core context.
        std::scoped_lock lk{lock};

        // On exit, we no longer have an active thread.
        running_thread = nullptr;
    }

    // Return the halt reason.
    return halt;
}

HaltReason ARM_NCE::StepJit() {
    return HaltReason::StepThread;
}

u32 ARM_NCE::GetSvcNumber() const {
    return guest_ctx.svc_swi;
}

ARM_NCE::ARM_NCE(System& system_, bool uses_wall_clock_, std::size_t core_index_)
    : ARM_Interface{system_, uses_wall_clock_}, core_index{core_index_} {
    guest_ctx.system = &system_;
}

ARM_NCE::~ARM_NCE() = default;

void ARM_NCE::Initialize() {
    thread_id = gettid();

    // Setup our signals
    static std::once_flag flag;
    std::call_once(flag, [] {
        using HandlerType = decltype(sigaction::sa_sigaction);

        sigset_t signal_mask;
        sigemptyset(&signal_mask);
        sigaddset(&signal_mask, ReturnToRunCodeByExceptionLevelChangeSignal);
        sigaddset(&signal_mask, BreakFromRunCodeSignal);
        sigaddset(&signal_mask, GuestFaultSignal);

        struct sigaction return_to_run_code_action {};
        return_to_run_code_action.sa_flags = SA_SIGINFO | SA_ONSTACK;
        return_to_run_code_action.sa_sigaction = reinterpret_cast<HandlerType>(
            &ARM_NCE::ReturnToRunCodeByExceptionLevelChangeSignalHandler);
        return_to_run_code_action.sa_mask = signal_mask;
        Common::SigAction(ReturnToRunCodeByExceptionLevelChangeSignal, &return_to_run_code_action,
                          nullptr);

        struct sigaction break_from_run_code_action {};
        break_from_run_code_action.sa_flags = SA_SIGINFO | SA_ONSTACK;
        break_from_run_code_action.sa_sigaction =
            reinterpret_cast<HandlerType>(&ARM_NCE::BreakFromRunCodeSignalHandler);
        break_from_run_code_action.sa_mask = signal_mask;
        Common::SigAction(BreakFromRunCodeSignal, &break_from_run_code_action, nullptr);

        struct sigaction fault_action {};
        fault_action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART;
        fault_action.sa_sigaction =
            reinterpret_cast<HandlerType>(&ARM_NCE::GuestFaultSignalHandler);
        fault_action.sa_mask = signal_mask;
        Common::SigAction(GuestFaultSignal, &fault_action, &g_orig_action);

        // Simplify call for g_orig_action.
        // These fields occupy the same space in memory, so this should be a no-op in practice.
        if (!(g_orig_action.sa_flags & SA_SIGINFO)) {
            g_orig_action.sa_sigaction =
                reinterpret_cast<decltype(g_orig_action.sa_sigaction)>(g_orig_action.sa_handler);
        }
    });
}

void ARM_NCE::SetPC(u64 pc) {
    guest_ctx.pc = pc;
}

u64 ARM_NCE::GetPC() const {
    return guest_ctx.pc;
}

u64 ARM_NCE::GetSP() const {
    return guest_ctx.sp;
}

u64 ARM_NCE::GetReg(int index) const {
    return guest_ctx.cpu_registers[index];
}

void ARM_NCE::SetReg(int index, u64 value) {
    guest_ctx.cpu_registers[index] = value;
}

u128 ARM_NCE::GetVectorReg(int index) const {
    return guest_ctx.vector_registers[index];
}

void ARM_NCE::SetVectorReg(int index, u128 value) {
    guest_ctx.vector_registers[index] = value;
}

u32 ARM_NCE::GetPSTATE() const {
    return guest_ctx.pstate;
}

void ARM_NCE::SetPSTATE(u32 pstate) {
    guest_ctx.pstate = pstate;
}

u64 ARM_NCE::GetTlsAddress() const {
    return guest_ctx.tpidrro_el0;
}

void ARM_NCE::SetTlsAddress(u64 address) {
    guest_ctx.tpidrro_el0 = address;
}

u64 ARM_NCE::GetTPIDR_EL0() const {
    return guest_ctx.tpidr_el0;
}

void ARM_NCE::SetTPIDR_EL0(u64 value) {
    guest_ctx.tpidr_el0 = value;
}

void ARM_NCE::SaveContext(ThreadContext64& ctx) const {
    ctx.cpu_registers = guest_ctx.cpu_registers;
    ctx.sp = guest_ctx.sp;
    ctx.pc = guest_ctx.pc;
    ctx.pstate = guest_ctx.pstate;
    ctx.vector_registers = guest_ctx.vector_registers;
    ctx.fpcr = guest_ctx.fpcr;
    ctx.fpsr = guest_ctx.fpsr;
    ctx.tpidr = guest_ctx.tpidr_el0;
}

void ARM_NCE::LoadContext(const ThreadContext64& ctx) {
    guest_ctx.cpu_registers = ctx.cpu_registers;
    guest_ctx.sp = ctx.sp;
    guest_ctx.pc = ctx.pc;
    guest_ctx.pstate = ctx.pstate;
    guest_ctx.vector_registers = ctx.vector_registers;
    guest_ctx.fpcr = ctx.fpcr;
    guest_ctx.fpsr = ctx.fpsr;
    guest_ctx.tpidr_el0 = ctx.tpidr;
}

void ARM_NCE::SignalInterrupt() {
    // Lock core context.
    std::scoped_lock lk{lock};

    // Add break loop condition.
    guest_ctx.esr_el1.fetch_or(static_cast<u64>(HaltReason::BreakLoop));

    // If there is no thread running, we are done.
    if (running_thread == nullptr) {
        return;
    }

    // Lock the thread context.
    auto* params = &running_thread->GetNativeExecutionParameters();
    LockThreadParameters(params);

    if (params->is_running) {
        // We should signal to the running thread.
        // The running thread will unlock the thread context.
        syscall(SYS_tkill, thread_id, BreakFromRunCodeSignal);
    } else {
        // If the thread is no longer running, we have nothing to do.
        UnlockThreadParameters(params);
    }
}

void ARM_NCE::ClearInterrupt() {
    guest_ctx.esr_el1 = {};
}

void ARM_NCE::ClearInstructionCache() {
    // TODO: This is not possible to implement correctly on Linux because
    // we do not have any access to ic iallu.

    // Require accesses to complete.
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

void ARM_NCE::InvalidateCacheRange(u64 addr, std::size_t size) {
    // Clean cache.
    auto* ptr = reinterpret_cast<char*>(addr);
    __builtin___clear_cache(ptr, ptr + size);
}

void ARM_NCE::ClearExclusiveState() {
    // No-op.
}

void ARM_NCE::PageTableChanged(Common::PageTable& page_table,
                               std::size_t new_address_space_size_in_bits) {
    // No-op. Page table is never used.
}

} // namespace Core
