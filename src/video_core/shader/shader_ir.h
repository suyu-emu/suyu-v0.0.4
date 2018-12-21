// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <set>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/shader_bytecode.h"
#include "video_core/engines/shader_header.h"

namespace VideoCommon::Shader {

class OperationNode;
class ConditionalNode;
class GprNode;
class ImmediateNode;
class InternalFlagNode;
class PredicateNode;
class AbufNode; ///< Attribute buffer
class CbufNode; ///< Constant buffer
class LmemNode; ///< Local memory
class GmemNode; ///< Global memory
class CommentNode;

using ProgramCode = std::vector<u64>;

using NodeData =
    std::variant<OperationNode, ConditionalNode, GprNode, ImmediateNode, InternalFlagNode,
                 PredicateNode, AbufNode, CbufNode, LmemNode, GmemNode, CommentNode>;
using Node = const NodeData*;
using BasicBlock = std::vector<Node>;

constexpr u32 MAX_PROGRAM_LENGTH = 0x1000;

constexpr u32 RZ = 0xff;

enum class OperationCode {
    Assign,          /// (float& dest, float src) -> void
    AssignComposite, /// (MetaComponents, float4 src, float&[4] dst) -> void

    Composite, /// (float[4] values) -> float4
    Select,    /// (MetaArithmetic, bool pred, float a, float b) -> float

    FAdd,          /// (MetaArithmetic, float a, float b) -> float
    FMul,          /// (MetaArithmetic, float a, float b) -> float
    FDiv,          /// (MetaArithmetic, float a, float b) -> float
    FFma,          /// (MetaArithmetic, float a, float b, float c) -> float
    FNegate,       /// (MetaArithmetic, float a) -> float
    FAbsolute,     /// (MetaArithmetic, float a) -> float
    FClamp,        /// (MetaArithmetic, float value, float min, float max) -> float
    FMin,          /// (MetaArithmetic, float a, float b) -> float
    FMax,          /// (MetaArithmetic, float a, float b) -> float
    FCos,          /// (MetaArithmetic, float a) -> float
    FSin,          /// (MetaArithmetic, float a) -> float
    FExp2,         /// (MetaArithmetic, float a) -> float
    FLog2,         /// (MetaArithmetic, float a) -> float
    FInverseSqrt,  /// (MetaArithmetic, float a) -> float
    FSqrt,         /// (MetaArithmetic, float a) -> float
    FRoundEven,    /// (MetaArithmetic, float a) -> float
    FFloor,        /// (MetaArithmetic, float a) -> float
    FCeil,         /// (MetaArithmetic, float a) -> float
    FTrunc,        /// (MetaArithmetic, float a) -> float
    FCastInteger,  /// (MetaArithmetic, int a) -> float
    FCastUInteger, /// (MetaArithmetic, uint a) -> float

    IAdd,                  /// (MetaArithmetic, int a, int b) -> int
    IMul,                  /// (MetaArithmetic, int a, int b) -> int
    IDiv,                  /// (MetaArithmetic, int a, int b) -> int
    INegate,               /// (MetaArithmetic, int a) -> int
    IAbsolute,             /// (MetaArithmetic, int a) -> int
    IMin,                  /// (MetaArithmetic, int a, int b) -> int
    IMax,                  /// (MetaArithmetic, int a, int b) -> int
    ICastFloat,            /// (MetaArithmetic, float a) -> int
    ICastUnsigned,         /// (MetaArithmetic, uint a) -> int
    ILogicalShiftLeft,     /// (MetaArithmetic, int a, uint b) -> int
    ILogicalShiftRight,    /// (MetaArithmetic, int a, uint b) -> int
    IArithmeticShiftRight, /// (MetaArithmetic, int a, uint b) -> int
    IBitwiseAnd,           /// (MetaArithmetic, int a, int b) -> int
    IBitwiseOr,            /// (MetaArithmetic, int a, int b) -> int
    IBitwiseXor,           /// (MetaArithmetic, int a, int b) -> int
    IBitwiseNot,           /// (MetaArithmetic, int a) -> int
    IBitfieldInsert,       /// (MetaArithmetic, int base, int insert, int offset, int bits) -> int

    UAdd,                  /// (MetaArithmetic, uint a, uint b) -> uint
    UMul,                  /// (MetaArithmetic, uint a, uint b) -> uint
    UDiv,                  /// (MetaArithmetic, uint a, uint b) -> uint
    UMin,                  /// (MetaArithmetic, uint a, uint b) -> uint
    UMax,                  /// (MetaArithmetic, uint a, uint b) -> uint
    UCastFloat,            /// (MetaArithmetic, float a) -> uint
    UCastSigned,           /// (MetaArithmetic, int a) -> uint
    ULogicalShiftLeft,     /// (MetaArithmetic, uint a, uint b) -> uint
    ULogicalShiftRight,    /// (MetaArithmetic, uint a, uint b) -> uint
    UArithmeticShiftRight, /// (MetaArithmetic, uint a, uint b) -> uint
    UBitwiseAnd,           /// (MetaArithmetic, uint a, uint b) -> uint
    UBitwiseOr,            /// (MetaArithmetic, uint a, uint b) -> uint
    UBitwiseXor,           /// (MetaArithmetic, uint a, uint b) -> uint
    UBitwiseNot,           /// (MetaArithmetic, uint a) -> int
    UBitfieldInsert, /// (MetaArithmetic, uint base, uint insert, int offset, int bits) -> uint

    HAdd,      /// (MetaHalfArithmetic, f16vec2 a, f16vec2 b) -> f16vec2
    HMul,      /// (MetaHalfArithmetic, f16vec2 a, f16vec2 b) -> f16vec2
    HAbsolute, /// (f16vec2 a) -> f16vec2
    HNegate,   /// (f16vec2 a, bool first, bool second) -> f16vec2
    HMergeF32, /// (f16vec2 src) -> float
    HMergeH0,  /// (f16vec2 dest, f16vec2 src) -> f16vec2
    HMergeH1,  /// (f16vec2 dest, f16vec2 src) -> f16vec2

    LogicalAssign, /// (bool& dst, bool src) -> void
    LogicalAnd,    /// (bool a, bool b) -> bool
    LogicalOr,     /// (bool a, bool b) -> bool
    LogicalXor,    /// (bool a, bool b) -> bool
    LogicalNegate, /// (bool a) -> bool

    LogicalFLessThan,     /// (float a, float b) -> bool
    LogicalFEqual,        /// (float a, float b) -> bool
    LogicalFLessEqual,    /// (float a, float b) -> bool
    LogicalFGreaterThan,  /// (float a, float b) -> bool
    LogicalFNotEqual,     /// (float a, float b) -> bool
    LogicalFGreaterEqual, /// (float a, float b) -> bool
    LogicalFIsNan,        /// (float a) -> bool

    LogicalILessThan,     /// (int a, int b) -> bool
    LogicalIEqual,        /// (int a, int b) -> bool
    LogicalILessEqual,    /// (int a, int b) -> bool
    LogicalIGreaterThan,  /// (int a, int b) -> bool
    LogicalINotEqual,     /// (int a, int b) -> bool
    LogicalIGreaterEqual, /// (int a, int b) -> bool

    LogicalULessThan,     /// (uint a, uint b) -> bool
    LogicalUEqual,        /// (uint a, uint b) -> bool
    LogicalULessEqual,    /// (uint a, uint b) -> bool
    LogicalUGreaterThan,  /// (uint a, uint b) -> bool
    LogicalUNotEqual,     /// (uint a, uint b) -> bool
    LogicalUGreaterEqual, /// (uint a, uint b) -> bool

    LogicalHLessThan,     /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool
    LogicalHEqual,        /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool
    LogicalHLessEqual,    /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool
    LogicalHGreaterThan,  /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool
    LogicalHNotEqual,     /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool
    LogicalHGreaterEqual, /// (MetaHalfArithmetic, f16vec2 a, f16vec2) -> bool

    F4Texture,                /// (MetaTexture, float[N] coords, float[M] params) -> float4
    F4TextureLod,             /// (MetaTexture, float[N] coords, float[M] params) -> float4
    F4TextureGather,          /// (MetaTexture, float[N] coords, float[M] params) -> float4
    F4TextureQueryDimensions, /// (MetaTexture, float a) -> float4
    F4TextureQueryLod,        /// (MetaTexture, float[N] coords) -> float4

    Ipa, /// (abuf src) -> float

    Bra,  /// (uint branch_target) -> void
    Ssy,  /// (uint branch_target) -> void
    Pbk,  /// (uint branch_target) -> void
    Sync, /// () -> void
    Brk,  /// () -> void
    Exit, /// () -> void
    Kil,  /// () -> void

    YNegate, /// () -> float

    Amount,
};

enum class InternalFlag {
    Zero = 0,
    Sign = 1,
    Carry = 2,
    Overflow = 3,
    Amount = 4,
};

/// Describes the behaviour of code path of a given entry point and a return point.
enum class ExitMethod {
    Undetermined, ///< Internal value. Only occur when analyzing JMP loop.
    AlwaysReturn, ///< All code paths reach the return point.
    Conditional,  ///< Code path reaches the return point or an END instruction conditionally.
    AlwaysEnd,    ///< All code paths reach a END instruction.
};

class Sampler {
public:
    explicit Sampler(std::size_t offset, std::size_t index, Tegra::Shader::TextureType type,
                     bool is_array, bool is_shadow)
        : offset{offset}, index{index}, type{type}, is_array{is_array}, is_shadow{is_shadow} {}

    std::size_t GetOffset() const {
        return offset;
    }

    u32 GetIndex() const {
        return static_cast<u32>(index);
    }

    Tegra::Shader::TextureType GetType() const {
        return type;
    }

    bool IsArray() const {
        return is_array;
    }

    bool IsShadow() const {
        return is_shadow;
    }

    bool operator<(const Sampler& rhs) const {
        return std::tie(offset, index, type, is_array, is_shadow) <
               std::tie(rhs.offset, rhs.index, rhs.type, rhs.is_array, rhs.is_shadow);
    }

private:
    /// Offset in TSC memory from which to read the sampler object, as specified by the sampling
    /// instruction.
    std::size_t offset{};
    std::size_t index{}; ///< Value used to index into the generated GLSL sampler array.
    Tegra::Shader::TextureType type{}; ///< The type used to sample this texture (Texture2D, etc)
    bool is_array{};  ///< Whether the texture is being sampled as an array texture or not.
    bool is_shadow{}; ///< Whether the texture is being sampled as a depth texture or not.
};

class ConstBuffer {
public:
    void MarkAsUsed(u64 offset) {
        max_offset = std::max(max_offset, static_cast<u32>(offset));
    }

    void MarkAsUsedIndirect() {
        is_indirect = true;
    }

    bool IsIndirect() const {
        return is_indirect;
    }

    u32 GetSize() const {
        return max_offset + 1;
    }

private:
    u32 max_offset{};
    bool is_indirect{};
};

struct MetaArithmetic {
    bool precise{};
};

struct MetaHalfArithmetic {
    bool precise{};
    std::array<Tegra::Shader::HalfType, 3> types = {Tegra::Shader::HalfType::H0_H1,
                                                    Tegra::Shader::HalfType::H0_H1,
                                                    Tegra::Shader::HalfType::H0_H1};
    bool and_comparison{};
};

struct MetaTexture {
    const Sampler& sampler;
    u32 coords_count{};
};

struct MetaComponents {
    std::array<u32, 4> components_map{};

    u32 GetSourceComponent(u32 dest_index) const {
        return components_map[dest_index];
    }
};

constexpr MetaArithmetic PRECISE = {true};
constexpr MetaArithmetic NO_PRECISE = {false};
constexpr MetaHalfArithmetic HALF_NO_PRECISE = {false};

using Meta = std::variant<MetaArithmetic, MetaHalfArithmetic, MetaTexture, MetaComponents>;

/// Holds any kind of operation that can be done in the IR
class OperationNode final {
public:
    template <typename... T>
    explicit constexpr OperationNode(OperationCode code) : code{code}, meta{} {}

    template <typename... T>
    explicit constexpr OperationNode(OperationCode code, Meta&& meta)
        : code{code}, meta{std::move(meta)} {}

    template <typename... T>
    explicit constexpr OperationNode(OperationCode code, const T*... operands)
        : OperationNode(code, {}, operands...) {}

    template <typename... T>
    explicit constexpr OperationNode(OperationCode code, Meta&& meta, const T*... operands_)
        : code{code}, meta{std::move(meta)} {

        auto operands_list = {operands_...};
        for (auto& operand : operands_list) {
            operands.push_back(operand);
        }
    }

    explicit OperationNode(OperationCode code, Meta&& meta, std::vector<Node>&& operands)
        : code{code}, meta{meta}, operands{std::move(operands)} {}

    explicit OperationNode(OperationCode code, std::vector<Node>&& operands)
        : code{code}, meta{}, operands{std::move(operands)} {}

    OperationCode GetCode() const {
        return code;
    }

    const Meta& GetMeta() const {
        return meta;
    }

    std::size_t GetOperandsCount() const {
        return operands.size();
    }

    Node operator[](std::size_t operand_index) const {
        return operands.at(operand_index);
    }

private:
    const OperationCode code;
    const Meta meta;
    std::vector<Node> operands;
};

/// Encloses inside any kind of node that returns a boolean conditionally-executed code
class ConditionalNode final {
public:
    explicit ConditionalNode(Node condition, std::vector<Node>&& code)
        : condition{condition}, code{std::move(code)} {}

    Node GetCondition() const {
        return condition;
    }

    const std::vector<Node>& GetCode() const {
        return code;
    }

private:
    const Node condition;   ///< Condition to be satisfied
    std::vector<Node> code; ///< Code to execute
};

/// A general purpose register
class GprNode final {
public:
    explicit constexpr GprNode(Tegra::Shader::Register index) : index{index} {}

    u32 GetIndex() const {
        return static_cast<u32>(index);
    }

private:
    const Tegra::Shader::Register index;
};

/// A 32-bits value that represents an immediate value
class ImmediateNode final {
public:
    explicit constexpr ImmediateNode(u32 value) : value{value} {}

    u32 GetValue() const {
        return value;
    }

private:
    const u32 value;
};

/// One of Maxwell's internal flags
class InternalFlagNode final {
public:
    explicit constexpr InternalFlagNode(InternalFlag flag) : flag{flag} {}

    InternalFlag GetFlag() const {
        return flag;
    }

private:
    const InternalFlag flag;
};

/// A predicate register, it can be negated without aditional nodes
class PredicateNode final {
public:
    explicit constexpr PredicateNode(Tegra::Shader::Pred index, bool negated)
        : index{index}, negated{negated} {}

    Tegra::Shader::Pred GetIndex() const {
        return index;
    }

    bool IsNegated() const {
        return negated;
    }

private:
    const Tegra::Shader::Pred index;
    const bool negated;
};

/// Attribute buffer memory (known as attributes or varyings in GLSL terms)
class AbufNode final {
public:
    explicit constexpr AbufNode(Tegra::Shader::Attribute::Index index, u32 element,
                                const Tegra::Shader::IpaMode& input_mode, Node buffer = {})
        : input_mode{input_mode}, index{index}, element{element}, buffer{buffer} {}

    explicit constexpr AbufNode(Tegra::Shader::Attribute::Index index, u32 element,
                                Node buffer = {})
        : input_mode{}, index{index}, element{element}, buffer{buffer} {}

    Tegra::Shader::IpaMode GetInputMode() const {
        return input_mode;
    }

    Tegra::Shader::Attribute::Index GetIndex() const {
        return index;
    }

    u32 GetElement() const {
        return element;
    }

    Node GetBuffer() const {
        return buffer;
    }

private:
    const Tegra::Shader::IpaMode input_mode;
    const Node buffer;
    const Tegra::Shader::Attribute::Index index;
    const u32 element;
};

/// Constant buffer node, usually mapped to uniform buffers in GLSL
class CbufNode final {
public:
    explicit constexpr CbufNode(u32 index, Node offset) : index{index}, offset{offset} {}

    u32 GetIndex() const {
        return index;
    }

    Node GetOffset() const {
        return offset;
    }

private:
    const u32 index;
    const Node offset;
};

/// Local memory node
class LmemNode final {
public:
    explicit constexpr LmemNode(Node address) : address{address} {}

    Node GetAddress() const {
        return address;
    }

private:
    const Node address;
};

/// Global memory node
class GmemNode final {
public:
    explicit GmemNode(Node address) : address{address} {}

    Node GetAddress() const {
        return address;
    }

private:
    const Node address;
};

/// Commentary, can be dropped
class CommentNode final {
public:
    explicit CommentNode(const std::string& text) : text{text} {}

    const std::string& GetText() const {
        return text;
    }

private:
    const std::string text;
};

class ShaderIR final {
public:
    explicit ShaderIR(const ProgramCode& program_code, u32 main_offset)
        : program_code{program_code}, main_offset{main_offset} {

        Decode();
    }

    const std::map<u32, BasicBlock>& GetBasicBlocks() const {
        return basic_blocks;
    }

    const std::set<u32>& GetRegisters() const {
        return used_registers;
    }

    const std::set<Tegra::Shader::Pred>& GetPredicates() const {
        return used_predicates;
    }

    const std::map<Tegra::Shader::Attribute::Index, std::set<Tegra::Shader::IpaMode>>&
    GetInputAttributes() const {
        return used_input_attributes;
    }

    const std::set<Tegra::Shader::Attribute::Index>& GetOutputAttributes() const {
        return used_output_attributes;
    }

    const std::map<u32, ConstBuffer>& GetConstantBuffers() const {
        return used_cbufs;
    }

    const std::set<Sampler>& GetSamplers() const {
        return used_samplers;
    }

    const std::array<bool, Tegra::Engines::Maxwell3D::Regs::NumClipDistances>& GetClipDistances()
        const {
        return used_clip_distances;
    }

    std::size_t GetLength() const {
        return static_cast<std::size_t>(coverage_end * sizeof(u64));
    }

    const Tegra::Shader::Header& GetHeader() const {
        return header;
    }

private:
    void Decode();

    ExitMethod Scan(u32 begin, u32 end, std::set<u32>& labels);

    BasicBlock DecodeRange(u32 begin, u32 end);

    /**
     * Decodes a single instruction from Tegra to IR.
     * @param bb Basic block where the nodes will be written to.
     * @param pc Program counter. Offset to decode.
     * @return Next address to decode.
     */
    u32 DecodeInstr(BasicBlock& bb, u32 pc);

    u32 DecodeArithmetic(BasicBlock& bb, u32 pc);
    u32 DecodeArithmeticImmediate(BasicBlock& bb, u32 pc);
    u32 DecodeBfe(BasicBlock& bb, u32 pc);
    u32 DecodeBfi(BasicBlock& bb, u32 pc);
    u32 DecodeShift(BasicBlock& bb, u32 pc);
    u32 DecodeArithmeticInteger(BasicBlock& bb, u32 pc);
    u32 DecodeArithmeticIntegerImmediate(BasicBlock& bb, u32 pc);
    u32 DecodeArithmeticHalf(BasicBlock& bb, u32 pc);
    u32 DecodeArithmeticHalfImmediate(BasicBlock& bb, u32 pc);
    u32 DecodeFfma(BasicBlock& bb, u32 pc);
    u32 DecodeHfma2(BasicBlock& bb, u32 pc);
    u32 DecodeConversion(BasicBlock& bb, u32 pc);
    u32 DecodeMemory(BasicBlock& bb, u32 pc);
    u32 DecodeFloatSetPredicate(BasicBlock& bb, u32 pc);
    u32 DecodeIntegerSetPredicate(BasicBlock& bb, u32 pc);
    u32 DecodeHalfSetPredicate(BasicBlock& bb, u32 pc);
    u32 DecodePredicateSetRegister(BasicBlock& bb, u32 pc);
    u32 DecodePredicateSetPredicate(BasicBlock& bb, u32 pc);
    u32 DecodeRegisterSetPredicate(BasicBlock& bb, u32 pc);
    u32 DecodeFloatSet(BasicBlock& bb, u32 pc);
    u32 DecodeIntegerSet(BasicBlock& bb, u32 pc);
    u32 DecodeHalfSet(BasicBlock& bb, u32 pc);
    u32 DecodeXmad(BasicBlock& bb, u32 pc);
    u32 DecodeOther(BasicBlock& bb, u32 pc);

    /// Internalizes node's data and returns a managed pointer to a clone of that node
    Node StoreNode(NodeData&& node_data);

    /// Creates a conditional node
    Node Conditional(Node condition, std::vector<Node>&& code);
    /// Creates a commentary
    Node Comment(const std::string& text);
    /// Creates an u32 immediate
    Node Immediate(u32 value);
    /// Creates a s32 immediate
    Node Immediate(s32 value) {
        return Immediate(static_cast<u32>(value));
    }
    /// Creates a f32 immediate
    Node Immediate(f32 value) {
        // TODO(Rodrigo): Replace this with bit_cast when C++20 releases
        return Immediate(*reinterpret_cast<const u32*>(&value));
    }

    /// Generates a node for a passed register.
    Node GetRegister(Tegra::Shader::Register reg);
    /// Generates a node representing a 19-bit immediate value
    Node GetImmediate19(Tegra::Shader::Instruction instr);
    /// Generates a node representing a 32-bit immediate value
    Node GetImmediate32(Tegra::Shader::Instruction instr);
    /// Generates a node representing a constant buffer
    Node GetConstBuffer(u64 index, u64 offset);
    /// Generates a node representing a constant buffer with a variadic offset
    Node GetConstBufferIndirect(u64 index, u64 offset, Node node);
    /// Generates a node for a passed predicate. It can be optionally negated
    Node GetPredicate(u64 pred, bool negated = false);
    /// Generates a predicate node for an immediate true or false value
    Node GetPredicate(bool immediate);
    /// Generates a node representing an input atttribute. Keeps track of used attributes.
    Node GetInputAttribute(Tegra::Shader::Attribute::Index index, u64 element,
                           const Tegra::Shader::IpaMode& input_mode, Node buffer = {});
    /// Generates a node representing an output atttribute. Keeps track of used attributes.
    Node GetOutputAttribute(Tegra::Shader::Attribute::Index index, u64 element, Node buffer);
    /// Generates a node representing an internal flag
    Node GetInternalFlag(InternalFlag flag, bool negated = false);
    /// Generates a node representing a local memory address
    Node GetLocalMemory(Node address);


    template <typename... T>
    inline Node Operation(OperationCode code, const T*... operands) {
        return StoreNode(OperationNode(code, operands...));
    }

    template <typename... T>
    inline Node Operation(OperationCode code, Meta&& meta, const T*... operands) {
        return StoreNode(OperationNode(code, std::move(meta), operands...));
    }

    template <typename... T>
    inline Node Operation(OperationCode code, std::vector<Node>&& operands) {
        return StoreNode(OperationNode(code, std::move(operands)));
    }

    template <typename... T>
    inline Node Operation(OperationCode code, Meta&& meta, std::vector<Node>&& operands) {
        return StoreNode(OperationNode(code, std::move(meta), std::move(operands)));
    }

    template <typename... T>
    inline Node SignedOperation(OperationCode code, bool is_signed, const T*... operands) {
        return StoreNode(OperationNode(SignedToUnsignedCode(code, is_signed), operands...));
    }

    template <typename... T>
    inline Node SignedOperation(OperationCode code, bool is_signed, Meta&& meta,
                                const T*... operands) {
        return StoreNode(
            OperationNode(SignedToUnsignedCode(code, is_signed), std::move(meta), operands...));
    }

    static OperationCode SignedToUnsignedCode(OperationCode operation_code, bool is_signed);

    const ProgramCode& program_code;
    const u32 main_offset;

    u32 coverage_begin{};
    u32 coverage_end{};
    std::map<std::pair<u32, u32>, ExitMethod> exit_method_map;

    std::map<u32, BasicBlock> basic_blocks;

    std::vector<std::unique_ptr<NodeData>> stored_nodes;

    std::set<u32> used_registers;
    std::set<Tegra::Shader::Pred> used_predicates;
    std::map<Tegra::Shader::Attribute::Index, std::set<Tegra::Shader::IpaMode>>
        used_input_attributes;
    std::set<Tegra::Shader::Attribute::Index> used_output_attributes;
    std::map<u32, ConstBuffer> used_cbufs;
    std::set<Sampler> used_samplers;
    std::array<bool, Tegra::Engines::Maxwell3D::Regs::NumClipDistances> used_clip_distances{};

    Tegra::Shader::Header header;
};

} // namespace VideoCommon::Shader