// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifdef __APPLE__
#    include <signal.h>
#    include <sys/ucontext.h>
#else
#    include <signal.h>
#    ifndef __OpenBSD__
#        include <ucontext.h>
#    endif
#    ifdef __sun__
// Thanks C macros for exisitng in Solaris headers, thanks a lot
// We really needed to define FOR EVERY SINGLE REGISTER didn't we?
#        include <sys/regset.h>
#        undef EAX
#        undef EBX
#        undef ECX
#        undef EDX
#        undef ESP
#        undef EBP
#        undef ESI
#        undef EDI
#        undef ERR
#        undef SS
#        undef CS
#        undef ES
#        undef DS
#    endif
#    ifdef __linux__
#       include <sys/syscall.h>
#    endif
#endif

#ifdef ARCHITECTURE_x86_64
#   ifdef __OpenBSD__
#       define CTX_DECLARE(raw_context) ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(raw_context);
#   else
#       define CTX_DECLARE(raw_context) \
        ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(raw_context);  \
        [[maybe_unused]] auto& mctx = ucontext->uc_mcontext;
#   endif
#elif defined(ARCHITECTURE_arm64)
#   ifdef __OpenBSD__
#       define CTX_DECLARE(raw_context) ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(raw_context);
#   else
#       define CTX_DECLARE(raw_context) \
        ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(raw_context);  \
        [[maybe_unused]] auto& mctx = ucontext->uc_mcontext; \
        [[maybe_unused]] const auto fpctx = GetFloatingPointState(mctx);
#   endif
#endif

#if defined(ARCHITECTURE_x86_64)
#    if defined(__APPLE__)
#        define CTX_RIP (mctx->__ss.__rip)
#        define CTX_RSP (mctx->__ss.__rsp)
#    elif defined(__linux__)
#        define CTX_RIP (mctx.gregs[REG_RIP])
#        define CTX_RSP (mctx.gregs[REG_RSP])
#    elif defined(__FreeBSD__)
#        define CTX_RIP (mctx.mc_rip)
#        define CTX_RSP (mctx.mc_rsp)
#    elif defined(__NetBSD__)
#        define CTX_RIP (mctx.__gregs[_REG_RIP])
#        define CTX_RSP (mctx.__gregs[_REG_RSP])
#    elif defined(__OpenBSD__)
#        define CTX_RIP (ucontext->sc_rip)
#        define CTX_RSP (ucontext->sc_rsp)
#    elif defined(__sun__)
#        define CTX_RIP (mctx.gregs[REG_RIP])
#        define CTX_RSP (mctx.gregs[REG_RSP])
#    elif defined(__DragonFly__)
#        define CTX_RIP (mctx.mc_rip)
#        define CTX_RSP (mctx.mc_rsp)
#    else
#        error "Unknown platform"
#    endif
#elif defined(ARCHITECTURE_arm64)
#    if defined(__APPLE__)
#        define CTX_PC (mctx->__ss.__pc)
#        define CTX_SP (mctx->__ss.__sp)
#        define CTX_LR (mctx->__ss.__lr)
#        define CTX_PSTATE (mctx->__ss.__cpsr)
#        define CTX_X(i) (mctx->__ss.__x[i])
#        define CTX_Q(i) (mctx->__ns.__v[i])
#        define CTX_FPSR (mctx->__ns.__fpsr)
#        define CTX_FPCR (mctx->__ns.__fpcr)
#    elif defined(__linux__)
#        define CTX_PC (mctx.pc)
#        define CTX_SP (mctx.sp)
#        define CTX_LR (mctx.regs[30])
#        define CTX_PSTATE (mctx.pstate)
#        define CTX_X(i) (mctx.regs[i])
#        define CTX_Q(i) (fpctx->vregs[i])
#        define CTX_FPSR (fpctx->fpsr)
#        define CTX_FPCR (fpctx->fpcr)
#    elif defined(__FreeBSD__)
#        define CTX_PC (mctx.mc_gpregs.gp_elr)
#        define CTX_SP (mctx.mc_gpregs.gp_sp)
#        define CTX_LR (mctx.mc_gpregs.gp_lr)
#        define CTX_X(i) (mctx.mc_gpregs.gp_x[i])
#        define CTX_Q(i) (mctx.mc_fpregs.fp_q[i])
#    elif defined(__NetBSD__)
#        define CTX_PC (mctx.mc_gpregs.gp_elr)
#        define CTX_SP (mctx.mc_gpregs.gp_sp)
#        define CTX_LR (mctx.mc_gpregs.gp_lr)
#        define CTX_X(i) (mctx.mc_gpregs.gp_x[i])
#        define CTX_Q(i) (mctx.mc_fpregs.fp_q[i])
#    elif defined(__OpenBSD__)
#        define CTX_PC (ucontext->sc_elr)
#        define CTX_SP (ucontext->sc_sp)
#        define CTX_LR (ucontext->sc_lr)
#        define CTX_X(i) (ucontext->sc_x[i])
#        define CTX_Q(i) (ucontext->sc_q[i])
#    else
#        error "Unknown platform"
#    endif
#else
#    error "unimplemented"
#endif

#ifdef ARCHITECTURE_arm64
#ifdef __APPLE__
inline _STRUCT_ARM_NEON_STATE64* GetFloatingPointState(mcontext_t& host_ctx) {
    return &(host_ctx->__ns);
}
#elif defined(__linux__)
inline fpsimd_context* GetFloatingPointState(mcontext_t& host_ctx) {
    _aarch64_ctx* header = reinterpret_cast<_aarch64_ctx*>(&host_ctx.__reserved);
    while (header->magic != FPSIMD_MAGIC)
        header = reinterpret_cast<_aarch64_ctx*>(reinterpret_cast<char*>(header) + header->size);
    return reinterpret_cast<fpsimd_context*>(header);
}
#endif
#endif
