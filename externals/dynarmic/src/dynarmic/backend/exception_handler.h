/* This file is part of the dynarmic project.
 * Copyright (c) 2020 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <functional>
#include <memory>
#include <optional>

#include <mcl/macro/architecture.hpp>
#include <mcl/stdint.hpp>

#if defined(MCL_ARCHITECTURE_X86_64)
namespace Dynarmic::Backend::X64 {
class BlockOfCode;
}  // namespace Dynarmic::Backend::X64
#elif defined(MCL_ARCHITECTURE_ARM64)
namespace oaknut {
class CodeBlock;
}  // namespace oaknut
#elif defined(MCL_ARCHITECTURE_RISCV)
namespace Dynarmic::Backend::RV64 {
class CodeBlock;
}  // namespace Dynarmic::Backend::RV64
#else
#    error "Invalid architecture"
#endif

namespace Dynarmic::Backend {

#if defined(MCL_ARCHITECTURE_X86_64)
struct FakeCall {
    u64 call_rip;
    u64 ret_rip;
};
#elif defined(MCL_ARCHITECTURE_ARM64)
struct FakeCall {
    u64 call_pc;
};
#elif defined(MCL_ARCHITECTURE_RISCV)
struct FakeCall {
};
#else
#    error "Invalid architecture"
#endif

class ExceptionHandler final {
public:
    ExceptionHandler();
    ~ExceptionHandler();

#if defined(MCL_ARCHITECTURE_X86_64)
    void Register(X64::BlockOfCode& code);
#elif defined(MCL_ARCHITECTURE_ARM64)
    void Register(oaknut::CodeBlock& mem, std::size_t mem_size);
#elif defined(MCL_ARCHITECTURE_RISCV)
    void Register(RV64::CodeBlock& mem, std::size_t mem_size);
#else
#    error "Invalid architecture"
#endif

    bool SupportsFastmem() const noexcept;
    void SetFastmemCallback(std::function<FakeCall(u64)> cb);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}  // namespace Dynarmic::Backend
