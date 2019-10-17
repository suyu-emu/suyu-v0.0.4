// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <optional>

#include "common/bit_field.h"
#include "common/common_types.h"

namespace Tegra {
namespace Engines {
class Maxwell3D;
}

class MacroInterpreter final {
public:
    explicit MacroInterpreter(Engines::Maxwell3D& maxwell3d);

    /**
     * Executes the macro code with the specified input parameters.
     * @param offset Offset to start execution at.
     * @param parameters The parameters of the macro.
     */
    void Execute(u32 offset, std::size_t num_parameters, const u32* parameters);

private:
    enum class ALUOperation : u32;
    enum class BranchCondition : u32;
    enum class ResultOperation : u32;

    union Opcode;

    union MethodAddress {
        u32 raw;
        BitField<0, 12, u32> address;
        BitField<12, 6, u32> increment;
    };

    /// Resets the execution engine state, zeroing registers, etc.
    void Reset();

    /**
     * Executes a single macro instruction located at the current program counter. Returns whether
     * the interpreter should keep running.
     * @param offset Offset to start execution at.
     * @param is_delay_slot Whether the current step is being executed due to a delay slot in a
     * previous instruction.
     */
    bool Step(u32 offset, bool is_delay_slot);

    /// Calculates the result of an ALU operation. src_a OP src_b;
    u32 GetALUResult(ALUOperation operation, u32 src_a, u32 src_b);

    /// Performs the result operation on the input result and stores it in the specified register
    /// (if necessary).
    void ProcessResult(ResultOperation operation, u32 reg, u32 result);

    /// Evaluates the branch condition and returns whether the branch should be taken or not.
    bool EvaluateBranchCondition(BranchCondition cond, u32 value) const;

    /// Reads an opcode at the current program counter location.
    Opcode GetOpcode(u32 offset) const;

    /// Returns the specified register's value. Register 0 is hardcoded to always return 0.
    u32 GetRegister(u32 register_id) const;

    /// Sets the register to the input value.
    void SetRegister(u32 register_id, u32 value);

    /// Sets the method address to use for the next Send instruction.
    void SetMethodAddress(u32 address);

    /// Calls a GPU Engine method with the input parameter.
    void Send(u32 value);

    /// Reads a GPU register located at the method address.
    u32 Read(u32 method) const;

    /// Returns the next parameter in the parameter queue.
    u32 FetchParameter();

    Engines::Maxwell3D& maxwell3d;

    /// Current program counter
    u32 pc;
    /// Program counter to execute at after the delay slot is executed.
    std::optional<u32> delayed_pc;

    static constexpr std::size_t NumMacroRegisters = 8;

    /// General purpose macro registers.
    std::array<u32, NumMacroRegisters> registers = {};

    /// Method address to use for the next Send instruction.
    MethodAddress method_address = {};

    /// Input parameters of the current macro.
    std::unique_ptr<u32[]> parameters;
    std::size_t num_parameters = 0;
    std::size_t parameters_capacity = 0;
    /// Index of the next parameter that will be fetched by the 'parm' instruction.
    u32 next_parameter_index = 0;

    bool carry_flag = false;
};
} // namespace Tegra
