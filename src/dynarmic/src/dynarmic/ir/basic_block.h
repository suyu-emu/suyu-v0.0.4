// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>

#include <boost/container/container_fwd.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/container/stable_vector.hpp>
#include <mcl/container/intrusive_list.hpp>
#include "dynarmic/common/common_types.h"

#include "dynarmic/ir/location_descriptor.h"
#include "dynarmic/ir/microinstruction.h"
#include "dynarmic/ir/terminal.h"
#include "dynarmic/ir/value.h"
#include "dynarmic/ir/dense_list.h"

namespace Dynarmic::IR {

enum class Cond;
enum class Opcode;

/// A basic block. It consists of zero or more instructions followed by exactly one terminal.
/// Note that this is a linear IR and not a pure tree-based IR: i.e.: there is an ordering to
/// the microinstructions. This only matters before chaining is done in order to correctly
/// order memory accesses.
class Block final {
public:
    //using instruction_list_type = dense_list<Inst>;
    using instruction_list_type = mcl::intrusive_list<Inst>;
    using size_type = instruction_list_type::size_type;
    using iterator = instruction_list_type::iterator;
    using const_iterator = instruction_list_type::const_iterator;
    using reverse_iterator = instruction_list_type::reverse_iterator;
    using const_reverse_iterator = instruction_list_type::const_reverse_iterator;

    explicit Block(const LocationDescriptor& location);
    ~Block() = default;
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    Block(Block&&) = default;
    Block& operator=(Block&&) = default;

    bool empty() const { return instructions.empty(); }
    size_type size() const { return instructions.size(); }

    Inst& front() { return instructions.front(); }
    const Inst& front() const { return instructions.front(); }

    Inst& back() { return instructions.back(); }
    const Inst& back() const { return instructions.back(); }

    iterator begin() { return instructions.begin(); }
    const_iterator begin() const { return instructions.begin(); }
    iterator end() { return instructions.end(); }
    const_iterator end() const { return instructions.end(); }

    reverse_iterator rbegin() { return instructions.rbegin(); }
    const_reverse_iterator rbegin() const { return instructions.rbegin(); }
    reverse_iterator rend() { return instructions.rend(); }
    const_reverse_iterator rend() const { return instructions.rend(); }

    const_iterator cbegin() const { return instructions.cbegin(); }
    const_iterator cend() const { return instructions.cend(); }

    const_reverse_iterator crbegin() const { return instructions.crbegin(); }
    const_reverse_iterator crend() const { return instructions.crend(); }

    /// Appends a new instruction to the end of this basic block,
    /// handling any allocations necessary to do so.
    /// @param op   Opcode representing the instruction to add.
    /// @param args A sequence of Value instances used as arguments for the instruction.
    inline void AppendNewInst(const Opcode opcode, const std::initializer_list<IR::Value> args) noexcept {
        PrependNewInst(end(), opcode, args);
    }
    iterator PrependNewInst(iterator insertion_point, Opcode op, std::initializer_list<Value> args) noexcept;

    /// Gets a mutable reference to the instruction list for this basic block.
    inline instruction_list_type& Instructions() noexcept {
        return instructions;
    }
    /// Gets an immutable reference to the instruction list for this basic block.
    inline const instruction_list_type& Instructions() const noexcept {
        return instructions;
    }

    /// Gets the starting location for this basic block.
    inline LocationDescriptor Location() const noexcept {
        return location;
    }
    /// Gets the end location for this basic block.
    inline LocationDescriptor EndLocation() const noexcept {
        return end_location;
    }
    /// Sets the end location for this basic block.
    inline void SetEndLocation(const LocationDescriptor& descriptor) noexcept {
        end_location = descriptor;
    }

    /// Gets the condition required to pass in order to execute this block.
    inline Cond GetCondition() const noexcept {
        return cond;
    }
    /// Sets the condition required to pass in order to execute this block.
    inline void SetCondition(Cond condition) noexcept {
        cond = condition;
    }

    /// Gets the location of the block to execute if the predicated condition fails.
    inline LocationDescriptor ConditionFailedLocation() const noexcept {
        return *cond_failed;
    }
    /// Sets the location of the block to execute if the predicated condition fails.
    inline void SetConditionFailedLocation(LocationDescriptor fail_location) noexcept {
        cond_failed = fail_location;
    }
    /// Determines whether or not a predicated condition failure block is present.
    inline bool HasConditionFailedLocation() const noexcept {
        return cond_failed.has_value();
    }

    /// Gets a mutable reference to the condition failed cycle count.
    inline size_t& ConditionFailedCycleCount() noexcept {
        return cond_failed_cycle_count;
    }
    /// Gets an immutable reference to the condition failed cycle count.
    inline const size_t& ConditionFailedCycleCount() const noexcept {
        return cond_failed_cycle_count;
    }

    /// Gets the terminal instruction for this basic block.
    inline Terminal GetTerminal() const noexcept {
        return terminal;
    }
    /// Sets the terminal instruction for this basic block.
    inline void SetTerminal(Terminal term) noexcept {
        ASSERT(!HasTerminal() && "Terminal has already been set.");
        terminal = std::move(term);
    }
    /// Replaces the terminal instruction for this basic block.
    inline void ReplaceTerminal(Terminal term) noexcept {
        ASSERT(HasTerminal() && "Terminal has not been set.");
        terminal = std::move(term);
    }
    /// Determines whether or not this basic block has a terminal instruction.
    inline bool HasTerminal() const noexcept {
        return terminal.which() != 0;
    }

    /// Gets a mutable reference to the cycle count for this basic block.
    inline size_t& CycleCount() noexcept {
        return cycle_count;
    }
    /// Gets an immutable reference to the cycle count for this basic block.
    inline const size_t& CycleCount() const noexcept {
        return cycle_count;
    }
private:
    /// "Hot cache" for small blocks so we don't call global allocator
    boost::container::static_vector<Inst, 14> inlined_inst;
    /// List of instructions in this block.
    instruction_list_type instructions;
    /// "Long/far" memory pool
    boost::container::stable_vector<boost::container::static_vector<Inst, 32>> pooled_inst;
    /// Block to execute next if `cond` did not pass.
    std::optional<LocationDescriptor> cond_failed = {};
    /// Description of the starting location of this block
    LocationDescriptor location;
    /// Description of the end location of this block
    LocationDescriptor end_location;
    /// Conditional to pass in order to execute this block
    Cond cond;
    /// Terminal instruction of this block.
    Terminal terminal = Term::Invalid{};
    /// Number of cycles this block takes to execute if the conditional fails.
    size_t cond_failed_cycle_count = 0;
    /// Number of cycles this block takes to execute.
    size_t cycle_count = 0;
};
static_assert(sizeof(Block) == 2048);

/// Returns a string representation of the contents of block. Intended for debugging.
std::string DumpBlock(const IR::Block& block) noexcept;

}  // namespace Dynarmic::IR
