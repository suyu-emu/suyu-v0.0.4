// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <array>
#include <functional>
#include <optional>

#include "boost/container/small_vector.hpp"
#include "dynarmic/common/common_types.h"
#include <xbyak/xbyak.h>
#include <boost/container/static_vector.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/pool/pool_alloc.hpp>

#include "dynarmic/backend/x64/block_of_code.h"
#include "dynarmic/backend/x64/hostloc.h"
#include "dynarmic/backend/x64/stack_layout.h"
#include "dynarmic/backend/x64/oparg.h"
#include "dynarmic/backend/x64/abi.h"
#include "dynarmic/ir/cond.h"
#include "dynarmic/ir/microinstruction.h"
#include "dynarmic/ir/value.h"

namespace Dynarmic::IR {
enum class AccType;
}  // namespace Dynarmic::IR

namespace Dynarmic::Backend::X64 {

class RegAlloc;

struct HostLocInfo {
public:
    HostLocInfo() {}
    inline bool IsLocked() const {
        return is_being_used_count > 0;
    }
    inline bool IsEmpty() const {
        return is_being_used_count == 0 && values.empty();
    }
    inline bool IsLastUse() const {
        return is_being_used_count == 0 && current_references == 1 && accumulated_uses + 1 == total_uses;
    }
    inline void SetLastUse() noexcept {
        ASSERT(IsLastUse());
        is_set_last_use = true;
    }
    inline void ReadLock() noexcept {
        ASSERT(size_t(is_being_used_count) + 1 < (std::numeric_limits<uint16_t>::max)());
        ASSERT(!is_scratch);
        is_being_used_count++;
    }
    inline void WriteLock() noexcept {
        ASSERT(size_t(is_being_used_count) + 1 < (std::numeric_limits<uint16_t>::max)());
        ASSERT(is_being_used_count == 0);
        is_being_used_count++;
        is_scratch = true;
    }
    inline void AddArgReference() noexcept {
        ASSERT(size_t(current_references) + 1 < (std::numeric_limits<uint16_t>::max)());
        current_references++;
        ASSERT(accumulated_uses + current_references <= total_uses);
    }
    void ReleaseOne() noexcept;
    void ReleaseAll() noexcept;

    /// Checks if the given instruction is in our values set
    /// SAFETY: Const is casted away, irrelevant since this is only used for checking
    inline bool ContainsValue(const IR::Inst* inst) const noexcept {
        //return values.contains(const_cast<IR::Inst*>(inst));
        return std::find(values.begin(), values.end(), inst) != values.end();
    }
    inline size_t GetMaxBitWidth() const noexcept {
        return 1 << max_bit_width;
    }
    void AddValue(IR::Inst* inst) noexcept;
    void EmitVerboseDebuggingOutput(BlockOfCode* code, size_t host_loc_index) const noexcept;
private:
//non trivial
    boost::container::small_vector<IR::Inst*, 3> values; //24
    // Block state
    uint16_t total_uses = 0; //8
    //sometimes zeroed
    uint16_t accumulated_uses = 0; //8
//always zeroed
    // Current instruction state
    uint16_t is_being_used_count = 0; //8
    uint16_t current_references = 0; //8
    // Value state
    uint8_t lru_counter : 2 = 0; //1
    uint8_t max_bit_width : 4 = 0; //Valid values: log2(1,2,4,8,16,32,128) = (0, 1, 2, 3, 4, 5, 6)
    bool is_scratch : 1 = false; //1
    bool is_set_last_use : 1 = false; //1
    friend class RegAlloc;
};
static_assert(sizeof(HostLocInfo) == 64);

struct Argument {
public:
    using copyable_reference = std::reference_wrapper<Argument>;

    inline IR::Type GetType() const noexcept {
        return value.GetType();
    }
    inline bool IsImmediate() const noexcept {
        return value.IsImmediate();
    }
    inline bool IsVoid() const noexcept {
        return GetType() == IR::Type::Void;
    }

    bool FitsInImmediateU32() const noexcept;
    bool FitsInImmediateS32() const noexcept;

    bool GetImmediateU1() const noexcept;
    u8 GetImmediateU8() const noexcept;
    u16 GetImmediateU16() const noexcept;
    u32 GetImmediateU32() const noexcept;
    u64 GetImmediateS32() const noexcept;
    u64 GetImmediateU64() const noexcept;
    IR::Cond GetImmediateCond() const noexcept;
    IR::AccType GetImmediateAccType() const noexcept;

    /// Is this value currently in a GPR?
    bool IsInGpr() const noexcept;
    bool IsInXmm() const noexcept;
    bool IsInMemory() const noexcept;
private:
    friend class RegAlloc;
    explicit Argument(RegAlloc& reg_alloc) : reg_alloc(reg_alloc) {}

//data
    IR::Value value; //8
    RegAlloc& reg_alloc; //8
    bool allocated = false; //1
};

class RegAlloc final {
public:
    using ArgumentInfo = std::array<Argument, IR::max_arg_count>;
    RegAlloc() noexcept = default;
    RegAlloc(BlockOfCode* code, boost::container::static_vector<HostLoc, 28> gpr_order, boost::container::static_vector<HostLoc, 28> xmm_order) noexcept;

    ArgumentInfo GetArgumentInfo(const IR::Inst* inst) noexcept;
    void RegisterPseudoOperation(const IR::Inst* inst) noexcept;
    inline bool IsValueLive(const IR::Inst* inst) const noexcept {
        return !!ValueLocation(inst);
    }
    inline Xbyak::Reg64 UseGpr(Argument& arg) noexcept {
        ASSERT(!arg.allocated);
        arg.allocated = true;
        return HostLocToReg64(UseImpl(arg.value, gpr_order));
    }
    inline Xbyak::Xmm UseXmm(Argument& arg) noexcept {
        ASSERT(!arg.allocated);
        arg.allocated = true;
        return HostLocToXmm(UseImpl(arg.value, xmm_order));
    }
    inline OpArg UseOpArg(Argument& arg) noexcept {
        return UseGpr(arg);
    }
    inline void Use(Argument& arg, const HostLoc host_loc) noexcept {
        ASSERT(!arg.allocated);
        arg.allocated = true;
        UseImpl(arg.value, {host_loc});
    }

    Xbyak::Reg64 UseScratchGpr(Argument& arg) noexcept;
    Xbyak::Xmm UseScratchXmm(Argument& arg) noexcept;
    void UseScratch(Argument& arg, HostLoc host_loc) noexcept;

    void DefineValue(IR::Inst* inst, const Xbyak::Reg& reg) noexcept;
    void DefineValue(IR::Inst* inst, Argument& arg) noexcept;

    void Release(const Xbyak::Reg& reg) noexcept;

    inline Xbyak::Reg64 ScratchGpr() noexcept {
        return HostLocToReg64(ScratchImpl(gpr_order));
    }
    inline Xbyak::Reg64 ScratchGpr(const HostLoc desired_location) noexcept {
        return HostLocToReg64(ScratchImpl({desired_location}));
    }
    inline Xbyak::Xmm ScratchXmm() noexcept {
        return HostLocToXmm(ScratchImpl(xmm_order));
    }
    inline Xbyak::Xmm ScratchXmm(HostLoc desired_location) noexcept {
        return HostLocToXmm(ScratchImpl({desired_location}));
    }

    void HostCall(IR::Inst* result_def = nullptr,
        const std::optional<Argument::copyable_reference> arg0 = {},
        const std::optional<Argument::copyable_reference> arg1 = {},
        const std::optional<Argument::copyable_reference> arg2 = {},
        const std::optional<Argument::copyable_reference> arg3 = {}
    ) noexcept;

    // TODO: Values in host flags
    void AllocStackSpace(const size_t stack_space) noexcept;
    void ReleaseStackSpace(const size_t stack_space) noexcept;

    inline void EndOfAllocScope() noexcept {
        for (auto& iter : hostloc_info) {
            iter.ReleaseAll();
        }
    }
    inline void AssertNoMoreUses() noexcept {
        ASSERT(std::all_of(hostloc_info.begin(), hostloc_info.end(), [](const auto& i) noexcept { return i.IsEmpty(); }));
    }
    inline void EmitVerboseDebuggingOutput() noexcept {
        for (size_t i = 0; i < hostloc_info.size(); i++) {
            hostloc_info[i].EmitVerboseDebuggingOutput(code, i);
        }
    }
private:
    friend struct Argument;

    HostLoc SelectARegister(const boost::container::static_vector<HostLoc, 28>& desired_locations) const noexcept;
    inline std::optional<HostLoc> ValueLocation(const IR::Inst* value) const noexcept {
        for (size_t i = 0; i < hostloc_info.size(); i++) {
            if (hostloc_info[i].ContainsValue(value)) {
                return HostLoc(i);
            }
        }
        return std::nullopt;
    }

    HostLoc UseImpl(IR::Value use_value, const boost::container::static_vector<HostLoc, 28>& desired_locations) noexcept;
    HostLoc UseScratchImpl(IR::Value use_value, const boost::container::static_vector<HostLoc, 28>& desired_locations) noexcept;
    HostLoc ScratchImpl(const boost::container::static_vector<HostLoc, 28>& desired_locations) noexcept;
    void DefineValueImpl(IR::Inst* def_inst, HostLoc host_loc) noexcept;
    void DefineValueImpl(IR::Inst* def_inst, const IR::Value& use_inst) noexcept;

    HostLoc LoadImmediate(IR::Value imm, HostLoc host_loc) noexcept;
    void Move(HostLoc to, HostLoc from) noexcept;
    void CopyToScratch(size_t bit_width, HostLoc to, HostLoc from) noexcept;
    void Exchange(HostLoc a, HostLoc b) noexcept;
    void MoveOutOfTheWay(HostLoc reg) noexcept;

    void SpillRegister(HostLoc loc) noexcept;
    HostLoc FindFreeSpill(bool is_xmm) const noexcept;

    inline HostLocInfo& LocInfo(const HostLoc loc) noexcept {
        ASSERT(loc != HostLoc::RSP && loc != ABI_JIT_PTR);
        return hostloc_info[static_cast<size_t>(loc)];
    }
    inline const HostLocInfo& LocInfo(const HostLoc loc) const noexcept {
        ASSERT(loc != HostLoc::RSP && loc != ABI_JIT_PTR);
        return hostloc_info[static_cast<size_t>(loc)];
    }

    void EmitMove(const size_t bit_width, const HostLoc to, const HostLoc from) noexcept;
    void EmitExchange(const HostLoc a, const HostLoc b) noexcept;

//data
    alignas(64) boost::container::static_vector<HostLoc, 28> gpr_order;
    alignas(64) boost::container::static_vector<HostLoc, 28> xmm_order;
    alignas(64) std::array<HostLocInfo, NonSpillHostLocCount + SpillCount> hostloc_info;
    BlockOfCode* code = nullptr;
    size_t reserved_stack_space = 0;
};
// Ensure a cache line (or less) is used, this is primordial
static_assert(sizeof(boost::container::static_vector<HostLoc, 28>) == 40);

}  // namespace Dynarmic::Backend::X64
