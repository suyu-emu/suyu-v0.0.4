// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2019 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <bit>
#include <fmt/format.h>
#include <ankerl/unordered_dense.h>
#include "dynarmic/backend/exception_handler.h"
#include "dynarmic/common/assert.h"
#include "dynarmic/common/context.h"
#include "dynarmic/common/common_types.h"
#if defined(ARCHITECTURE_x86_64)
#    include "dynarmic/backend/x64/block_of_code.h"
#elif defined(ARCHITECTURE_arm64)
#    include <oaknut/code_block.hpp>
#    include "dynarmic/backend/arm64/abi.h"
#elif defined(ARCHITECTURE_riscv64)
#    include "dynarmic/backend/riscv64/code_block.h"
#else
#    error "Invalid architecture"
#endif

namespace Dynarmic::Backend {

namespace {

struct CodeBlockInfo {
    u64 size;
    std::function<FakeCall(u64)> cb;
};

class SigHandler {
    auto FindCodeBlockInfo(u64 offset) noexcept {
        return std::find_if(code_block_infos.begin(), code_block_infos.end(), [&](auto const& e) {
            return e.first <= offset && e.first + e.second.size > offset;
        });
    }
    static void SigAction(int sig, siginfo_t* info, void* raw_context);

    bool supports_fast_mem = true;
    void* signal_stack_memory = nullptr;
    ankerl::unordered_dense::map<u64, CodeBlockInfo> code_block_infos;
    std::shared_mutex code_block_infos_mutex;
    struct sigaction old_sa_segv;
    struct sigaction old_sa_bus;
    std::size_t signal_stack_size;
public:
    SigHandler() noexcept {
        signal_stack_size = std::max<size_t>(SIGSTKSZ, 2 * 1024 * 1024);
        signal_stack_memory = mmap(nullptr, signal_stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        stack_t signal_stack{};
        signal_stack.ss_sp = signal_stack_memory;
        signal_stack.ss_size = signal_stack_size;
        signal_stack.ss_flags = 0;
        if (sigaltstack(&signal_stack, nullptr) != 0) {
            fmt::print(stderr, "dynarmic: POSIX SigHandler: init failure at sigaltstack\n");
            supports_fast_mem = false;
            return;
        }

        struct sigaction sa{};
        sa.sa_handler = nullptr;
        sa.sa_sigaction = &SigHandler::SigAction;
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGSEGV, &sa, &old_sa_segv) != 0) {
            fmt::print(stderr, "dynarmic: POSIX SigHandler: could not set SIGSEGV handler\n");
            supports_fast_mem = false;
            return;
        }
#ifdef __APPLE__
        if (sigaction(SIGBUS, &sa, &old_sa_bus) != 0) {
            fmt::print(stderr, "dynarmic: POSIX SigHandler: could not set SIGBUS handler\n");
            supports_fast_mem = false;
            return;
        }
#endif
    }

    ~SigHandler() noexcept {
        munmap(signal_stack_memory, signal_stack_size);
    }

    void AddCodeBlock(u64 offset, CodeBlockInfo cbi) noexcept {
        std::unique_lock guard(code_block_infos_mutex);
        code_block_infos.insert_or_assign(offset, cbi);
    }
    void RemoveCodeBlock(u64 offset) noexcept {
        std::unique_lock guard(code_block_infos_mutex);
        code_block_infos.erase(offset);
    }

    bool SupportsFastmem() const noexcept { return supports_fast_mem; }
};

std::mutex handler_lock;
std::optional<SigHandler> sig_handler;

void RegisterHandler() {
    std::lock_guard<std::mutex> guard(handler_lock);
    if (!sig_handler) {
        sig_handler.emplace();
    }
}

void SigHandler::SigAction(int sig, siginfo_t* info, void* raw_context) {
    DEBUG_ASSERT(sig == SIGSEGV || sig == SIGBUS);
    CTX_DECLARE(raw_context);
#if defined(ARCHITECTURE_x86_64)
    {
        std::shared_lock guard(sig_handler->code_block_infos_mutex);
        if (auto const iter = sig_handler->FindCodeBlockInfo(CTX_RIP); iter != sig_handler->code_block_infos.end()) {
            FakeCall fc = iter->second.cb(CTX_RIP);
            CTX_RSP -= sizeof(u64);
            *std::bit_cast<u64*>(CTX_RSP) = fc.ret_rip;
            CTX_RIP = fc.call_rip;
            return;
        }
    }
    fmt::print(stderr, "Unhandled {} at rip {:#018x}\n", sig == SIGSEGV ? "SIGSEGV" : "SIGBUS", CTX_RIP);
#elif defined(ARCHITECTURE_arm64)
    {
        std::shared_lock guard(sig_handler->code_block_infos_mutex);
        if (const auto iter = sig_handler->FindCodeBlockInfo(CTX_PC); iter != sig_handler->code_block_infos.end()) {
            FakeCall fc = iter->second.cb(CTX_PC);
            CTX_PC = fc.call_pc;
            return;
        }
    }
    fmt::print(stderr, "Unhandled {} at pc {:#018x}\n", sig == SIGSEGV ? "SIGSEGV" : "SIGBUS", CTX_PC);
#elif defined(ARCHITECTURE_riscv64)
    UNREACHABLE();
#else
#    error "Invalid architecture"
#endif

    struct sigaction* retry_sa = sig == SIGSEGV ? &sig_handler->old_sa_segv : &sig_handler->old_sa_bus;
    if (retry_sa->sa_flags & SA_SIGINFO) {
        retry_sa->sa_sigaction(sig, info, raw_context);
        return;
    }
    if (retry_sa->sa_handler == SIG_DFL) {
        signal(sig, SIG_DFL);
        return;
    }
    if (retry_sa->sa_handler == SIG_IGN) {
        return;
    }
    retry_sa->sa_handler(sig);
}

}  // anonymous namespace

struct ExceptionHandler::Impl final {
    Impl(u64 offset_, u64 size_)
            : offset(offset_)
            , size(size_) {
        RegisterHandler();
    }

    void SetCallback(std::function<FakeCall(u64)> cb) {
        sig_handler->AddCodeBlock(offset, CodeBlockInfo{
            .size = size,
            .cb = cb
        });
    }

    ~Impl() {
        sig_handler->RemoveCodeBlock(offset);
    }

private:
    u64 offset;
    u64 size;
};

ExceptionHandler::ExceptionHandler() = default;
ExceptionHandler::~ExceptionHandler() = default;

#if defined(ARCHITECTURE_x86_64)
void ExceptionHandler::Register(X64::BlockOfCode& code) {
    impl = std::make_unique<Impl>(std::bit_cast<u64>(code.getCode()), code.GetTotalCodeSize());
}
#elif defined(ARCHITECTURE_arm64)
void ExceptionHandler::Register(oaknut::CodeBlock& mem, std::size_t size) {
    impl = std::make_unique<Impl>(std::bit_cast<u64>(mem.ptr()), size);
}
#elif defined(ARCHITECTURE_riscv64)
void ExceptionHandler::Register(RV64::CodeBlock& mem, std::size_t size) {
    impl = std::make_unique<Impl>(std::bit_cast<u64>(mem.ptr<u64>()), size);
}
#else
#    error "Invalid architecture"
#endif

bool ExceptionHandler::SupportsFastmem() const noexcept {
    return bool(impl) && sig_handler->SupportsFastmem();
}

void ExceptionHandler::SetFastmemCallback(std::function<FakeCall(u64)> cb) {
    impl->SetCallback(cb);
}

}  // namespace Dynarmic::Backend
