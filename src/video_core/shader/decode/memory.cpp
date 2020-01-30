// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <vector>
#include <fmt/format.h>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "video_core/engines/shader_bytecode.h"
#include "video_core/shader/node_helper.h"
#include "video_core/shader/shader_ir.h"

namespace VideoCommon::Shader {

using Tegra::Shader::AtomicOp;
using Tegra::Shader::AtomicType;
using Tegra::Shader::Attribute;
using Tegra::Shader::GlobalAtomicOp;
using Tegra::Shader::GlobalAtomicType;
using Tegra::Shader::Instruction;
using Tegra::Shader::OpCode;
using Tegra::Shader::Register;
using Tegra::Shader::StoreType;

namespace {

bool IsUnaligned(Tegra::Shader::UniformType uniform_type) {
    return uniform_type == Tegra::Shader::UniformType::UnsignedByte ||
           uniform_type == Tegra::Shader::UniformType::UnsignedShort;
}

u32 GetUnalignedMask(Tegra::Shader::UniformType uniform_type) {
    switch (uniform_type) {
    case Tegra::Shader::UniformType::UnsignedByte:
        return 0b11;
    case Tegra::Shader::UniformType::UnsignedShort:
        return 0b10;
    default:
        UNREACHABLE();
        return 0;
    }
}

u32 GetMemorySize(Tegra::Shader::UniformType uniform_type) {
    switch (uniform_type) {
    case Tegra::Shader::UniformType::UnsignedByte:
        return 8;
    case Tegra::Shader::UniformType::UnsignedShort:
        return 16;
    case Tegra::Shader::UniformType::Single:
        return 32;
    case Tegra::Shader::UniformType::Double:
        return 64;
    case Tegra::Shader::UniformType::Quad:
    case Tegra::Shader::UniformType::UnsignedQuad:
        return 128;
    default:
        UNIMPLEMENTED_MSG("Unimplemented size={}!", static_cast<u32>(uniform_type));
        return 32;
    }
}

Node ExtractUnaligned(Node value, Node address, u32 mask, u32 size) {
    Node offset = Operation(OperationCode::UBitwiseAnd, address, Immediate(mask));
    offset = Operation(OperationCode::ULogicalShiftLeft, std::move(offset), Immediate(3));
    return Operation(OperationCode::UBitfieldExtract, std::move(value), std::move(offset),
                     Immediate(size));
}

Node InsertUnaligned(Node dest, Node value, Node address, u32 mask, u32 size) {
    Node offset = Operation(OperationCode::UBitwiseAnd, std::move(address), Immediate(mask));
    offset = Operation(OperationCode::ULogicalShiftLeft, std::move(offset), Immediate(3));
    return Operation(OperationCode::UBitfieldInsert, std::move(dest), std::move(value),
                     std::move(offset), Immediate(size));
}

Node Sign16Extend(Node value) {
    Node sign = Operation(OperationCode::UBitwiseAnd, value, Immediate(1U << 15));
    Node is_sign = Operation(OperationCode::LogicalUEqual, std::move(sign), Immediate(1U << 15));
    Node extend = Operation(OperationCode::Select, is_sign, Immediate(0xFFFF0000), Immediate(0));
    return Operation(OperationCode::UBitwiseOr, std::move(value), std::move(extend));
}

} // Anonymous namespace

u32 ShaderIR::DecodeMemory(NodeBlock& bb, u32 pc) {
    const Instruction instr = {program_code[pc]};
    const auto opcode = OpCode::Decode(instr);

    switch (opcode->get().GetId()) {
    case OpCode::Id::LD_A: {
        // Note: Shouldn't this be interp mode flat? As in no interpolation made.
        UNIMPLEMENTED_IF_MSG(instr.gpr8.Value() != Register::ZeroIndex,
                             "Indirect attribute loads are not supported");
        UNIMPLEMENTED_IF_MSG((instr.attribute.fmt20.immediate.Value() % sizeof(u32)) != 0,
                             "Unaligned attribute loads are not supported");
        UNIMPLEMENTED_IF_MSG(instr.attribute.fmt20.IsPhysical() &&
                                 instr.attribute.fmt20.size != Tegra::Shader::AttributeSize::Word,
                             "Non-32 bits PHYS reads are not implemented");

        const Node buffer{GetRegister(instr.gpr39)};

        u64 next_element = instr.attribute.fmt20.element;
        auto next_index = static_cast<u64>(instr.attribute.fmt20.index.Value());

        const auto LoadNextElement = [&](u32 reg_offset) {
            const Node attribute{instr.attribute.fmt20.IsPhysical()
                                     ? GetPhysicalInputAttribute(instr.gpr8, buffer)
                                     : GetInputAttribute(static_cast<Attribute::Index>(next_index),
                                                         next_element, buffer)};

            SetRegister(bb, instr.gpr0.Value() + reg_offset, attribute);

            // Load the next attribute element into the following register. If the element
            // to load goes beyond the vec4 size, load the first element of the next
            // attribute.
            next_element = (next_element + 1) % 4;
            next_index = next_index + (next_element == 0 ? 1 : 0);
        };

        const u32 num_words = static_cast<u32>(instr.attribute.fmt20.size.Value()) + 1;
        for (u32 reg_offset = 0; reg_offset < num_words; ++reg_offset) {
            LoadNextElement(reg_offset);
        }
        break;
    }
    case OpCode::Id::LD_C: {
        UNIMPLEMENTED_IF(instr.ld_c.unknown != 0);

        Node index = GetRegister(instr.gpr8);

        const Node op_a =
            GetConstBufferIndirect(instr.cbuf36.index, instr.cbuf36.GetOffset() + 0, index);

        switch (instr.ld_c.type.Value()) {
        case Tegra::Shader::UniformType::Single:
            SetRegister(bb, instr.gpr0, op_a);
            break;

        case Tegra::Shader::UniformType::Double: {
            const Node op_b =
                GetConstBufferIndirect(instr.cbuf36.index, instr.cbuf36.GetOffset() + 4, index);

            SetTemporary(bb, 0, op_a);
            SetTemporary(bb, 1, op_b);
            SetRegister(bb, instr.gpr0, GetTemporary(0));
            SetRegister(bb, instr.gpr0.Value() + 1, GetTemporary(1));
            break;
        }
        default:
            UNIMPLEMENTED_MSG("Unhandled type: {}", static_cast<unsigned>(instr.ld_c.type.Value()));
        }
        break;
    }
    case OpCode::Id::LD_L:
        LOG_DEBUG(HW_GPU, "LD_L cache management mode: {}", static_cast<u64>(instr.ld_l.unknown));
        [[fallthrough]];
    case OpCode::Id::LD_S: {
        const auto GetAddress = [&](s32 offset) {
            ASSERT(offset % 4 == 0);
            const Node immediate_offset = Immediate(static_cast<s32>(instr.smem_imm) + offset);
            return Operation(OperationCode::IAdd, GetRegister(instr.gpr8), immediate_offset);
        };
        const auto GetMemory = [&](s32 offset) {
            return opcode->get().GetId() == OpCode::Id::LD_S ? GetSharedMemory(GetAddress(offset))
                                                             : GetLocalMemory(GetAddress(offset));
        };

        switch (instr.ldst_sl.type.Value()) {
        case StoreType::Signed16:
            SetRegister(bb, instr.gpr0,
                        Sign16Extend(ExtractUnaligned(GetMemory(0), GetAddress(0), 0b10, 16)));
            break;
        case StoreType::Bits32:
        case StoreType::Bits64:
        case StoreType::Bits128: {
            const u32 count = [&] {
                switch (instr.ldst_sl.type.Value()) {
                case StoreType::Bits32:
                    return 1;
                case StoreType::Bits64:
                    return 2;
                case StoreType::Bits128:
                    return 4;
                default:
                    UNREACHABLE();
                    return 0;
                }
            }();
            for (u32 i = 0; i < count; ++i) {
                SetTemporary(bb, i, GetMemory(i * 4));
            }
            for (u32 i = 0; i < count; ++i) {
                SetRegister(bb, instr.gpr0.Value() + i, GetTemporary(i));
            }
            break;
        }
        default:
            UNIMPLEMENTED_MSG("{} Unhandled type: {}", opcode->get().GetName(),
                              static_cast<u32>(instr.ldst_sl.type.Value()));
        }
        break;
    }
    case OpCode::Id::LD:
    case OpCode::Id::LDG: {
        const auto type = [instr, &opcode]() -> Tegra::Shader::UniformType {
            switch (opcode->get().GetId()) {
            case OpCode::Id::LD:
                UNIMPLEMENTED_IF_MSG(!instr.generic.extended, "Unextended LD is not implemented");
                return instr.generic.type;
            case OpCode::Id::LDG:
                return instr.ldg.type;
            default:
                UNREACHABLE();
                return {};
            }
        }();

        const auto [real_address_base, base_address, descriptor] =
            TrackGlobalMemory(bb, instr, true, false);

        const u32 size = GetMemorySize(type);
        const u32 count = Common::AlignUp(size, 32) / 32;
        if (!real_address_base || !base_address) {
            // Tracking failed, load zeroes.
            for (u32 i = 0; i < count; ++i) {
                SetRegister(bb, instr.gpr0.Value() + i, Immediate(0.0f));
            }
            break;
        }

        for (u32 i = 0; i < count; ++i) {
            const Node it_offset = Immediate(i * 4);
            const Node real_address = Operation(OperationCode::UAdd, real_address_base, it_offset);
            Node gmem = MakeNode<GmemNode>(real_address, base_address, descriptor);

            // To handle unaligned loads get the bytes used to dereference global memory and extract
            // those bytes from the loaded u32.
            if (IsUnaligned(type)) {
                gmem = ExtractUnaligned(gmem, real_address, GetUnalignedMask(type), size);
            }

            SetTemporary(bb, i, gmem);
        }

        for (u32 i = 0; i < count; ++i) {
            SetRegister(bb, instr.gpr0.Value() + i, GetTemporary(i));
        }
        break;
    }
    case OpCode::Id::ST_A: {
        UNIMPLEMENTED_IF_MSG(instr.gpr8.Value() != Register::ZeroIndex,
                             "Indirect attribute loads are not supported");
        UNIMPLEMENTED_IF_MSG((instr.attribute.fmt20.immediate.Value() % sizeof(u32)) != 0,
                             "Unaligned attribute loads are not supported");

        u64 element = instr.attribute.fmt20.element;
        auto index = static_cast<u64>(instr.attribute.fmt20.index.Value());

        const u32 num_words = static_cast<u32>(instr.attribute.fmt20.size.Value()) + 1;
        for (u32 reg_offset = 0; reg_offset < num_words; ++reg_offset) {
            Node dest;
            if (instr.attribute.fmt20.patch) {
                const u32 offset = static_cast<u32>(index) * 4 + static_cast<u32>(element);
                dest = MakeNode<PatchNode>(offset);
            } else {
                dest = GetOutputAttribute(static_cast<Attribute::Index>(index), element,
                                          GetRegister(instr.gpr39));
            }
            const auto src = GetRegister(instr.gpr0.Value() + reg_offset);

            bb.push_back(Operation(OperationCode::Assign, dest, src));

            // Load the next attribute element into the following register. If the element to load
            // goes beyond the vec4 size, load the first element of the next attribute.
            element = (element + 1) % 4;
            index = index + (element == 0 ? 1 : 0);
        }
        break;
    }
    case OpCode::Id::ST_L:
        LOG_DEBUG(HW_GPU, "ST_L cache management mode: {}",
                  static_cast<u64>(instr.st_l.cache_management.Value()));
        [[fallthrough]];
    case OpCode::Id::ST_S: {
        const auto GetAddress = [&](s32 offset) {
            ASSERT(offset % 4 == 0);
            const Node immediate = Immediate(static_cast<s32>(instr.smem_imm) + offset);
            return Operation(OperationCode::IAdd, NO_PRECISE, GetRegister(instr.gpr8), immediate);
        };

        const bool is_local = opcode->get().GetId() == OpCode::Id::ST_L;
        const auto set_memory = is_local ? &ShaderIR::SetLocalMemory : &ShaderIR::SetSharedMemory;
        const auto get_memory = is_local ? &ShaderIR::GetLocalMemory : &ShaderIR::GetSharedMemory;

        switch (instr.ldst_sl.type.Value()) {
        case StoreType::Bits128:
            (this->*set_memory)(bb, GetAddress(12), GetRegister(instr.gpr0.Value() + 3));
            (this->*set_memory)(bb, GetAddress(8), GetRegister(instr.gpr0.Value() + 2));
            [[fallthrough]];
        case StoreType::Bits64:
            (this->*set_memory)(bb, GetAddress(4), GetRegister(instr.gpr0.Value() + 1));
            [[fallthrough]];
        case StoreType::Bits32:
            (this->*set_memory)(bb, GetAddress(0), GetRegister(instr.gpr0));
            break;
        case StoreType::Signed16: {
            Node address = GetAddress(0);
            Node memory = (this->*get_memory)(address);
            (this->*set_memory)(
                bb, address, InsertUnaligned(memory, GetRegister(instr.gpr0), address, 0b10, 16));
            break;
        }
        default:
            UNIMPLEMENTED_MSG("{} unhandled type: {}", opcode->get().GetName(),
                              static_cast<u32>(instr.ldst_sl.type.Value()));
        }
        break;
    }
    case OpCode::Id::ST:
    case OpCode::Id::STG: {
        const auto type = [instr, &opcode]() -> Tegra::Shader::UniformType {
            switch (opcode->get().GetId()) {
            case OpCode::Id::ST:
                UNIMPLEMENTED_IF_MSG(!instr.generic.extended, "Unextended ST is not implemented");
                return instr.generic.type;
            case OpCode::Id::STG:
                return instr.stg.type;
            default:
                UNREACHABLE();
                return {};
            }
        }();

        // For unaligned reads we have to read memory too.
        const bool is_read = IsUnaligned(type);
        const auto [real_address_base, base_address, descriptor] =
            TrackGlobalMemory(bb, instr, is_read, true);
        if (!real_address_base || !base_address) {
            // Tracking failed, skip the store.
            break;
        }

        const u32 size = GetMemorySize(type);
        const u32 count = Common::AlignUp(size, 32) / 32;
        for (u32 i = 0; i < count; ++i) {
            const Node it_offset = Immediate(i * 4);
            const Node real_address = Operation(OperationCode::UAdd, real_address_base, it_offset);
            const Node gmem = MakeNode<GmemNode>(real_address, base_address, descriptor);
            Node value = GetRegister(instr.gpr0.Value() + i);

            if (IsUnaligned(type)) {
                const u32 mask = GetUnalignedMask(type);
                value = InsertUnaligned(gmem, std::move(value), real_address, mask, size);
            }

            bb.push_back(Operation(OperationCode::Assign, gmem, value));
        }
        break;
    }
    case OpCode::Id::ATOM: {
        UNIMPLEMENTED_IF_MSG(instr.atom.operation != GlobalAtomicOp::Add, "operation={}",
                             static_cast<int>(instr.atom.operation.Value()));
        UNIMPLEMENTED_IF_MSG(instr.atom.type != GlobalAtomicType::S32, "type={}",
                             static_cast<int>(instr.atom.type.Value()));

        const auto [real_address, base_address, descriptor] =
            TrackGlobalMemory(bb, instr, true, true);
        if (!real_address || !base_address) {
            // Tracking failed, skip atomic.
            break;
        }

        Node gmem = MakeNode<GmemNode>(real_address, base_address, descriptor);
        Node value = Operation(OperationCode::AtomicAdd, std::move(gmem), GetRegister(instr.gpr20));
        SetRegister(bb, instr.gpr0, std::move(value));
        break;
    }
    case OpCode::Id::ATOMS: {
        UNIMPLEMENTED_IF_MSG(instr.atoms.operation != AtomicOp::Add, "operation={}",
                             static_cast<int>(instr.atoms.operation.Value()));
        UNIMPLEMENTED_IF_MSG(instr.atoms.type != AtomicType::U32, "type={}",
                             static_cast<int>(instr.atoms.type.Value()));

        const s32 offset = instr.atoms.GetImmediateOffset();
        Node address = GetRegister(instr.gpr8);
        address = Operation(OperationCode::IAdd, std::move(address), Immediate(offset));

        Node memory = GetSharedMemory(std::move(address));
        Node data = GetRegister(instr.gpr20);

        Node value = Operation(OperationCode::AtomicAdd, std::move(memory), std::move(data));
        SetRegister(bb, instr.gpr0, std::move(value));
        break;
    }
    case OpCode::Id::AL2P: {
        // Ignore al2p.direction since we don't care about it.

        // Calculate emulation fake physical address.
        const Node fixed_address{Immediate(static_cast<u32>(instr.al2p.address))};
        const Node reg{GetRegister(instr.gpr8)};
        const Node fake_address{Operation(OperationCode::IAdd, NO_PRECISE, reg, fixed_address)};

        // Set the fake address to target register.
        SetRegister(bb, instr.gpr0, fake_address);

        // Signal the shader IR to declare all possible attributes and varyings
        uses_physical_attributes = true;
        break;
    }
    default:
        UNIMPLEMENTED_MSG("Unhandled memory instruction: {}", opcode->get().GetName());
    }

    return pc;
}

std::tuple<Node, Node, GlobalMemoryBase> ShaderIR::TrackGlobalMemory(NodeBlock& bb,
                                                                     Instruction instr,
                                                                     bool is_read, bool is_write) {
    const auto addr_register{GetRegister(instr.gmem.gpr)};
    const auto immediate_offset{static_cast<u32>(instr.gmem.offset)};

    const auto [base_address, index, offset] =
        TrackCbuf(addr_register, global_code, static_cast<s64>(global_code.size()));
    ASSERT_OR_EXECUTE_MSG(base_address != nullptr,
                          { return std::make_tuple(nullptr, nullptr, GlobalMemoryBase{}); },
                          "Global memory tracking failed");

    bb.push_back(Comment(fmt::format("Base address is c[0x{:x}][0x{:x}]", index, offset)));

    const GlobalMemoryBase descriptor{index, offset};
    const auto& [entry, is_new] = used_global_memory.try_emplace(descriptor);
    auto& usage = entry->second;
    usage.is_written |= is_write;
    usage.is_read |= is_read;

    const auto real_address =
        Operation(OperationCode::UAdd, NO_PRECISE, Immediate(immediate_offset), addr_register);

    return {real_address, base_address, descriptor};
}

} // namespace VideoCommon::Shader
