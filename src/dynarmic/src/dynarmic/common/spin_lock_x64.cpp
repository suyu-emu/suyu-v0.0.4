// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2022 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <mutex>
#include <optional>
#include <xbyak/xbyak.h>

#include "dynarmic/backend/x64/abi.h"
#include "dynarmic/backend/x64/hostloc.h"
#include "dynarmic/common/spin_lock.h"

#ifdef DYNARMIC_ENABLE_NO_EXECUTE_SUPPORT
static const auto default_cg_mode = Xbyak::DontSetProtectRWE;
#else
static const auto default_cg_mode = nullptr; //Allow RWE
#endif

namespace Dynarmic {

void EmitSpinLockLock(Xbyak::CodeGenerator& code, Xbyak::Reg64 ptr, Xbyak::Reg32 tmp, bool waitpkg) {
    // TODO: this is because we lack regalloc - so better to be safe :(
    if (waitpkg) {
        code.push(Xbyak::util::eax);
        code.push(Xbyak::util::ebx);
        code.push(Xbyak::util::edx);
    }
    Xbyak::Label start, loop;
    code.jmp(start, code.T_NEAR);
    code.L(loop);
    if (waitpkg) {
        // TODO: This clobbers EAX and EDX did we tell the regalloc?
        // ARM ptr for address-monitoring
        code.umonitor(ptr);
        // tmp.bit[0] = 0: C0.1 | Slow Wakup | Better Savings
        // tmp.bit[0] = 1: C0.2 | Fast Wakup | Lesser Savings
        // edx:eax is implicitly used as a 64-bit deadline timestamp
        // Use the maximum so that we use the operating system's maximum
        // allowed wait time within the IA32_UMWAIT_CONTROL register
        // Enter power state designated by tmp and wait for a write to lock_ptr
        code.mov(Xbyak::util::eax, 0xFFFFFFFF);
        code.mov(Xbyak::util::edx, Xbyak::util::eax);
        // TODO: We can only be here because tmp is 1 already - however we repeatedly overwrite it...
        code.mov(Xbyak::util::ebx, 1);
        code.umwait(Xbyak::util::ebx);
        // CF == 1 if we hit the OS-timeout in IA32_UMWAIT_CONTROL without a write
        // CF == 0 if we exited the wait for any other reason
    } else {
        code.pause();
    }
    code.L(start);
    code.mov(tmp, 1);
    /*code.lock();*/ code.xchg(code.dword[ptr], tmp);
    code.test(tmp, tmp);
    code.jnz(loop, code.T_NEAR);
    if (waitpkg) {
        code.pop(Xbyak::util::edx);
        code.pop(Xbyak::util::ebx);
        code.pop(Xbyak::util::eax);
    }
}

void EmitSpinLockUnlock(Xbyak::CodeGenerator& code, Xbyak::Reg64 ptr, Xbyak::Reg32 tmp) {
    code.xor_(tmp, tmp);
    code.xchg(code.dword[ptr], tmp);
    code.mfence();
}

namespace {
struct SpinLockImpl {
    void Initialize() noexcept;
    static void GlobalInitialize() noexcept;
    Xbyak::CodeGenerator code = Xbyak::CodeGenerator(4096, default_cg_mode);
    void (*lock)(volatile int*) = nullptr;
    void (*unlock)(volatile int*) = nullptr;
};

std::once_flag flag;
/// @brief Bear in mind that initializing the variable as-is on ctor time will trigger bugs
/// because some OSes do not prepare mprotect() properly at static ctor time
/// We can't really do anything about it, so just live with this fact
std::optional<SpinLockImpl> impl;

void SpinLockImpl::Initialize() noexcept {
    Xbyak::Reg64 const ABI_PARAM1 = Backend::X64::HostLocToReg64(Backend::X64::ABI_PARAM1);
    code.align();
    lock = code.getCurr<void (*)(volatile int*)>();
    EmitSpinLockLock(code, ABI_PARAM1, code.eax, false);
    code.ret();
    code.align();
    unlock = code.getCurr<void (*)(volatile int*)>();
    EmitSpinLockUnlock(code, ABI_PARAM1, code.eax);
    code.ret();
}

void SpinLockImpl::GlobalInitialize() noexcept {
    impl.emplace();
    impl->Initialize();
}
}  // namespace

void SpinLock::Lock() noexcept {
    std::call_once(flag, &SpinLockImpl::GlobalInitialize);
    impl->lock(&storage);
}

void SpinLock::Unlock() noexcept {
    std::call_once(flag, &SpinLockImpl::GlobalInitialize);
    impl->unlock(&storage);
}

}  // namespace Dynarmic
