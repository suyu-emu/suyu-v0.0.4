// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2020 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include "dynarmic/backend/x64/abi.h"

#include <algorithm>

#include "dynarmic/common/common_types.h"
#include <xbyak/xbyak.h>

#include "dynarmic/backend/x64/block_of_code.h"

namespace Dynarmic::Backend::X64 {

constexpr size_t XMM_SIZE = 16;

struct FrameInfo {
    std::size_t stack_subtraction;
    std::size_t xmm_offset;
    std::size_t frame_offset;
};
static_assert(ABI_SHADOW_SPACE <= 32);

static FrameInfo CalculateFrameInfo(const size_t num_gprs, const size_t num_xmms, size_t frame_size) {
    // We are initially 8 byte aligned because the return value is pushed onto an aligned stack after a call.
    const size_t rsp_alignment = (num_gprs % 2 == 0) ? 8 : 0;
    const size_t total_xmm_size = num_xmms * XMM_SIZE;
    if (frame_size & 0xF) {
        frame_size += 0x10 - (frame_size & 0xF);
    }
    return {
        rsp_alignment + total_xmm_size + frame_size + ABI_SHADOW_SPACE,
        frame_size + ABI_SHADOW_SPACE,
        ABI_SHADOW_SPACE,
    };
}

template<typename RegisterArrayT>
void ABI_PushRegistersAndAdjustStack(BlockOfCode& code, const size_t frame_size, const RegisterArrayT& regs) {
    using namespace Xbyak::util;

    const size_t num_gprs = std::count_if(regs.begin(), regs.end(), HostLocIsGPR);
    const size_t num_xmms = std::count_if(regs.begin(), regs.end(), HostLocIsXMM);
    const FrameInfo frame_info = CalculateFrameInfo(num_gprs, num_xmms, frame_size);

    for (auto const gpr : regs)
        if (HostLocIsGPR(gpr))
            code.push(HostLocToReg64(gpr));
    if (frame_info.stack_subtraction != 0)
        code.sub(rsp, u32(frame_info.stack_subtraction));
    size_t xmm_offset = frame_info.xmm_offset;
    for (auto const xmm : regs) {
        if (HostLocIsXMM(xmm)) {
            if (code.HasHostFeature(HostFeature::AVX)) {
                code.vmovaps(code.xword[rsp + xmm_offset], HostLocToXmm(xmm));
            } else {
                code.movaps(code.xword[rsp + xmm_offset], HostLocToXmm(xmm));
            }
            xmm_offset += XMM_SIZE;
        }
    }
}

template<typename RegisterArrayT>
void ABI_PopRegistersAndAdjustStack(BlockOfCode& code, const size_t frame_size, const RegisterArrayT& regs) {
    using namespace Xbyak::util;

    const size_t num_gprs = std::count_if(regs.begin(), regs.end(), HostLocIsGPR);
    const size_t num_xmms = std::count_if(regs.begin(), regs.end(), HostLocIsXMM);
    const FrameInfo frame_info = CalculateFrameInfo(num_gprs, num_xmms, frame_size);

    size_t xmm_offset = frame_info.xmm_offset + (num_xmms * XMM_SIZE);
    for (auto it = regs.rbegin(); it != regs.rend(); ++it) {
        auto const xmm = *it;
        if (HostLocIsXMM(xmm)) {
            xmm_offset -= XMM_SIZE;
            if (code.HasHostFeature(HostFeature::AVX)) {
                code.vmovaps(HostLocToXmm(xmm), code.xword[rsp + xmm_offset]);
            } else {
                code.movaps(HostLocToXmm(xmm), code.xword[rsp + xmm_offset]);
            }
        }
    }
    if (frame_info.stack_subtraction != 0)
        code.add(rsp, u32(frame_info.stack_subtraction));
    for (auto it = regs.rbegin(); it != regs.rend(); ++it) {
        auto const gpr = *it;
        if (HostLocIsGPR(gpr))
            code.pop(HostLocToReg64(gpr));
    }
}

void ABI_PushCalleeSaveRegistersAndAdjustStack(BlockOfCode& code, const std::size_t frame_size) {
    ABI_PushRegistersAndAdjustStack(code, frame_size, ABI_ALL_CALLEE_SAVE);
}

void ABI_PopCalleeSaveRegistersAndAdjustStack(BlockOfCode& code, const std::size_t frame_size) {
    ABI_PopRegistersAndAdjustStack(code, frame_size, ABI_ALL_CALLEE_SAVE);
}

void ABI_PushCallerSaveRegistersAndAdjustStack(BlockOfCode& code, const std::size_t frame_size) {
    ABI_PushRegistersAndAdjustStack(code, frame_size, ABI_ALL_CALLER_SAVE);
}

void ABI_PopCallerSaveRegistersAndAdjustStack(BlockOfCode& code, const std::size_t frame_size) {
    ABI_PopRegistersAndAdjustStack(code, frame_size, ABI_ALL_CALLER_SAVE);
}

// Windows ABI registers are not in the same allocation algorithm as unix's
#ifdef _MSC_VER
void ABI_PushCallerSaveRegistersAndAdjustStackExcept(BlockOfCode& code, const HostLoc exception) {
    std::vector<HostLoc> regs;
    std::remove_copy(ABI_ALL_CALLER_SAVE.begin(), ABI_ALL_CALLER_SAVE.end(), std::back_inserter(regs), exception);
    ABI_PushRegistersAndAdjustStack(code, 0, regs);
}

void ABI_PopCallerSaveRegistersAndAdjustStackExcept(BlockOfCode& code, const HostLoc exception) {
    std::vector<HostLoc> regs;
    std::remove_copy(ABI_ALL_CALLER_SAVE.begin(), ABI_ALL_CALLER_SAVE.end(), std::back_inserter(regs), exception);
    ABI_PopRegistersAndAdjustStack(code, 0, regs);
}
#else
static consteval size_t ABI_AllCallerSaveSize() noexcept {
    return ABI_ALL_CALLER_SAVE.max_size();
}
static consteval std::array<HostLoc, ABI_AllCallerSaveSize() - 1> ABI_AllCallerSaveExcept(const std::size_t except) noexcept {
    std::array<HostLoc, ABI_AllCallerSaveSize() - 1> arr;
    for(std::size_t i = 0; i < arr.size(); ++i) {
        arr[i] = static_cast<HostLoc>(i + (i >= except ? 1 : 0));
    }
    return arr;
}

alignas(64) static constinit std::array<HostLoc, ABI_AllCallerSaveSize() - 1> ABI_CALLER_SAVED_EXCEPT_TABLE[32] = {
    ABI_AllCallerSaveExcept(0),
    ABI_AllCallerSaveExcept(1),
    ABI_AllCallerSaveExcept(2),
    ABI_AllCallerSaveExcept(3),
    ABI_AllCallerSaveExcept(4),
    ABI_AllCallerSaveExcept(5),
    ABI_AllCallerSaveExcept(6),
    ABI_AllCallerSaveExcept(7),
    ABI_AllCallerSaveExcept(8),
    ABI_AllCallerSaveExcept(9),
    ABI_AllCallerSaveExcept(10),
    ABI_AllCallerSaveExcept(11),
    ABI_AllCallerSaveExcept(12),
    ABI_AllCallerSaveExcept(13),
    ABI_AllCallerSaveExcept(14),
    ABI_AllCallerSaveExcept(15),
    ABI_AllCallerSaveExcept(16),
    ABI_AllCallerSaveExcept(17),
    ABI_AllCallerSaveExcept(18),
    ABI_AllCallerSaveExcept(19),
    ABI_AllCallerSaveExcept(20),
    ABI_AllCallerSaveExcept(21),
    ABI_AllCallerSaveExcept(22),
    ABI_AllCallerSaveExcept(23),
    ABI_AllCallerSaveExcept(24),
    ABI_AllCallerSaveExcept(25),
    ABI_AllCallerSaveExcept(26),
    ABI_AllCallerSaveExcept(27),
    ABI_AllCallerSaveExcept(28),
    ABI_AllCallerSaveExcept(29),
    ABI_AllCallerSaveExcept(30),
    ABI_AllCallerSaveExcept(31),
};

void ABI_PushCallerSaveRegistersAndAdjustStackExcept(BlockOfCode& code, const HostLoc exception) {
    ASSUME(size_t(exception) < 32);
    ABI_PushRegistersAndAdjustStack(code, 0, ABI_CALLER_SAVED_EXCEPT_TABLE[size_t(exception)]);
}

void ABI_PopCallerSaveRegistersAndAdjustStackExcept(BlockOfCode& code, const HostLoc exception) {
    ASSUME(size_t(exception) < 32);
    ABI_PopRegistersAndAdjustStack(code, 0, ABI_CALLER_SAVED_EXCEPT_TABLE[size_t(exception)]);
}
#endif

}  // namespace Dynarmic::Backend::X64
