
#include <list>
#include <map>
#include <unordered_set>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/shader/control_flow.h"
#include "video_core/shader/shader_ir.h"

namespace VideoCommon::Shader {

using Tegra::Shader::Instruction;
using Tegra::Shader::OpCode;

constexpr s32 unassigned_branch = -2;

struct BlockBranchInfo {
    Condition condition{};
    s32 address{exit_branch};
    bool kill{};
    bool is_sync{};
    bool is_brk{};
};

struct BlockInfo {
    BlockInfo() {}
    u32 start{};
    u32 end{};
    bool visited{};
    BlockBranchInfo branch{};

    bool IsInside(const u32 address) const {
        return start <= address && address <= end;
    }
};

struct Stamp {
    Stamp() = default;
    Stamp(u32 address, u32 target) : address{address}, target{target} {}
    u32 address{};
    u32 target{};
    bool operator==(const Stamp& sb) const {
        return std::tie(address, target) == std::tie(sb.address, sb.target);
    }
    bool operator<(const Stamp& sb) const {
        return address < sb.address;
    }
    bool operator>(const Stamp& sb) const {
        return address > sb.address;
    }
    bool operator<=(const Stamp& sb) const {
        return address <= sb.address;
    }
    bool operator>=(const Stamp& sb) const {
        return address >= sb.address;
    }
};

struct CFGRebuildState {
    explicit CFGRebuildState(const ProgramCode& program_code, const std::size_t program_size)
        : program_code{program_code}, program_size{program_size} {
        // queries.clear();
        block_info.clear();
        labels.clear();
        visited_address.clear();
        ssy_labels.clear();
        pbk_labels.clear();
        inspect_queries.clear();
    }

    std::vector<BlockInfo> block_info{};
    std::list<u32> inspect_queries{};
    // std::list<Query> queries{};
    std::unordered_set<u32> visited_address{};
    std::unordered_set<u32> labels{};
    std::set<Stamp> ssy_labels;
    std::set<Stamp> pbk_labels;
    const ProgramCode& program_code;
    const std::size_t program_size;
};

enum class BlockCollision : u32 { None = 0, Found = 1, Inside = 2 };

std::pair<BlockCollision, std::vector<BlockInfo>::iterator> TryGetBlock(CFGRebuildState& state,
                                                                        u32 address) {
    auto it = state.block_info.begin();
    while (it != state.block_info.end()) {
        if (it->start == address) {
            return {BlockCollision::Found, it};
        }
        if (it->IsInside(address)) {
            return {BlockCollision::Inside, it};
        }
        it++;
    }
    return {BlockCollision::None, it};
}

struct ParseInfo {
    BlockBranchInfo branch_info{};
    u32 end_address{};
};

BlockInfo* CreateBlockInfo(CFGRebuildState& state, u32 start, u32 end) {
    auto& it = state.block_info.emplace_back();
    it.start = start;
    it.end = end;
    state.visited_address.insert(start);
    return &it;
}

Pred GetPredicate(u32 index, bool negated) {
    return static_cast<Pred>(index + (negated ? 8 : 0));
}

enum class ParseResult : u32 {
    ControlCaught = 0,
    BlockEnd = 1,
    AbnormalFlow = 2,
};

ParseResult ParseCode(CFGRebuildState& state, u32 address, ParseInfo& parse_info) {

    u32 offset = static_cast<u32>(address);
    u32 end_address = static_cast<u32>(state.program_size - 10U) * 8U;

    auto insert_label = ([](CFGRebuildState& state, u32 address) {
        auto pair = state.labels.emplace(address);
        if (pair.second) {
            state.inspect_queries.push_back(address);
        }
    });

    while (true) {
        if (offset >= end_address) {
            parse_info.branch_info.address = exit_branch;
            break;
        }
        if (state.visited_address.count(offset) != 0) {
            parse_info.branch_info.address = offset;
            break;
        }
        const Instruction instr = {state.program_code[offset]};
        const auto opcode = OpCode::Decode(instr);
        if (!opcode || opcode->get().GetType() != OpCode::Type::Flow) {
            offset++;
            continue;
        }

        switch (opcode->get().GetId()) {
        case OpCode::Id::EXIT: {
            const auto pred_index = static_cast<u32>(instr.pred.pred_index);
            parse_info.branch_info.condition.predicate =
                GetPredicate(pred_index, instr.negate_pred != 0);
            if (parse_info.branch_info.condition.predicate == Pred::NeverExecute) {
                offset++;
                continue;
            }
            const ConditionCode cc = instr.flow_condition_code;
            parse_info.branch_info.condition.cc = cc;
            if (cc == ConditionCode::F) {
                offset++;
                continue;
            }
            parse_info.branch_info.address = exit_branch;
            parse_info.branch_info.kill = false;
            parse_info.branch_info.is_sync = false;
            parse_info.branch_info.is_brk = false;
            parse_info.end_address = offset;

            return ParseResult::ControlCaught;
        }
        case OpCode::Id::BRA: {
            if (instr.bra.constant_buffer != 0) {
                return ParseResult::AbnormalFlow;
            }
            const auto pred_index = static_cast<u32>(instr.pred.pred_index);
            parse_info.branch_info.condition.predicate =
                GetPredicate(pred_index, instr.negate_pred != 0);
            if (parse_info.branch_info.condition.predicate == Pred::NeverExecute) {
                offset++;
                continue;
            }
            const ConditionCode cc = instr.flow_condition_code;
            parse_info.branch_info.condition.cc = cc;
            if (cc == ConditionCode::F) {
                offset++;
                continue;
            }
            u32 branch_offset = offset + instr.bra.GetBranchTarget();
            if (branch_offset == 0) {
                parse_info.branch_info.address = exit_branch;
            } else {
                parse_info.branch_info.address = branch_offset;
            }
            insert_label(state, branch_offset);
            parse_info.branch_info.kill = false;
            parse_info.branch_info.is_sync = false;
            parse_info.branch_info.is_brk = false;
            parse_info.end_address = offset;

            return ParseResult::ControlCaught;
        }
        case OpCode::Id::SYNC: {
            parse_info.branch_info.condition;
            const auto pred_index = static_cast<u32>(instr.pred.pred_index);
            parse_info.branch_info.condition.predicate =
                GetPredicate(pred_index, instr.negate_pred != 0);
            if (parse_info.branch_info.condition.predicate == Pred::NeverExecute) {
                offset++;
                continue;
            }
            const ConditionCode cc = instr.flow_condition_code;
            parse_info.branch_info.condition.cc = cc;
            if (cc == ConditionCode::F) {
                offset++;
                continue;
            }
            parse_info.branch_info.address = unassigned_branch;
            parse_info.branch_info.kill = false;
            parse_info.branch_info.is_sync = true;
            parse_info.branch_info.is_brk = false;
            parse_info.end_address = offset;

            return ParseResult::ControlCaught;
        }
        case OpCode::Id::BRK: {
            parse_info.branch_info.condition;
            const auto pred_index = static_cast<u32>(instr.pred.pred_index);
            parse_info.branch_info.condition.predicate =
                GetPredicate(pred_index, instr.negate_pred != 0);
            if (parse_info.branch_info.condition.predicate == Pred::NeverExecute) {
                offset++;
                continue;
            }
            const ConditionCode cc = instr.flow_condition_code;
            parse_info.branch_info.condition.cc = cc;
            if (cc == ConditionCode::F) {
                offset++;
                continue;
            }
            parse_info.branch_info.address = unassigned_branch;
            parse_info.branch_info.kill = false;
            parse_info.branch_info.is_sync = false;
            parse_info.branch_info.is_brk = true;
            parse_info.end_address = offset;

            return ParseResult::ControlCaught;
        }
        case OpCode::Id::KIL: {
            parse_info.branch_info.condition;
            const auto pred_index = static_cast<u32>(instr.pred.pred_index);
            parse_info.branch_info.condition.predicate =
                GetPredicate(pred_index, instr.negate_pred != 0);
            if (parse_info.branch_info.condition.predicate == Pred::NeverExecute) {
                offset++;
                continue;
            }
            const ConditionCode cc = instr.flow_condition_code;
            parse_info.branch_info.condition.cc = cc;
            if (cc == ConditionCode::F) {
                offset++;
                continue;
            }
            parse_info.branch_info.address = exit_branch;
            parse_info.branch_info.kill = true;
            parse_info.branch_info.is_sync = false;
            parse_info.branch_info.is_brk = false;
            parse_info.end_address = offset;

            return ParseResult::ControlCaught;
        }
        case OpCode::Id::SSY: {
            const u32 target = offset + instr.bra.GetBranchTarget();
            insert_label(state, target);
            state.ssy_labels.emplace(offset, target);
            break;
        }
        case OpCode::Id::PBK: {
            const u32 target = offset + instr.bra.GetBranchTarget();
            insert_label(state, target);
            state.pbk_labels.emplace(offset, target);
            break;
        }
        default:
            break;
        }

        offset++;
    }
    parse_info.branch_info.kill = false;
    parse_info.branch_info.is_sync = false;
    parse_info.branch_info.is_brk = false;
    parse_info.end_address = offset - 1;
    return ParseResult::BlockEnd;
}

bool TryInspectAddress(CFGRebuildState& state) {
    if (state.inspect_queries.empty()) {
        return false;
    }
    u32 address = state.inspect_queries.front();
    state.inspect_queries.pop_front();
    auto search_result = TryGetBlock(state, address);
    BlockInfo* block_info;
    switch (search_result.first) {
    case BlockCollision::Found: {
        return true;
        break;
    }
    case BlockCollision::Inside: {
        // This case is the tricky one:
        // We need to Split the block in 2 sepprate blocks
        auto it = search_result.second;
        block_info = CreateBlockInfo(state, address, it->end);
        it->end = address - 1;
        block_info->branch = it->branch;
        BlockBranchInfo forward_branch{};
        forward_branch.address = address;
        it->branch = forward_branch;
        return true;
        break;
    }
    default:
        break;
    }
    ParseInfo parse_info;
    ParseResult parse_result = ParseCode(state, address, parse_info);
    if (parse_result == ParseResult::AbnormalFlow) {
        // if it's the end of the program, end it safely
        // if it's AbnormalFlow, we end it as false, ending the CFG reconstruction
        return false;
    }

    block_info = CreateBlockInfo(state, address, parse_info.end_address);
    block_info->branch = parse_info.branch_info;
    if (parse_info.branch_info.condition.IsUnconditional()) {
        return true;
    }

    u32 fallthrough_address = parse_info.end_address + 1;
    state.inspect_queries.push_front(fallthrough_address);
    return true;
}

bool ScanFlow(const ProgramCode& program_code, u32 program_size, u32 start_address,
              ShaderCharacteristics& result_out) {
    CFGRebuildState state{program_code, program_size};
    // Inspect Code and generate blocks
    state.labels.clear();
    state.labels.emplace(start_address);
    state.inspect_queries.push_back(start_address);
    while (!state.inspect_queries.empty()) {
        if (!TryInspectAddress(state)) {
            return false;
        }
    }
    std::sort(state.block_info.begin(), state.block_info.end(),
              [](const BlockInfo& a, const BlockInfo& b) -> bool { return a.start < b.start; });
    // Remove unvisited blocks
    result_out.blocks.clear();
    result_out.decompilable = false;
    result_out.start = start_address;
    result_out.end = start_address;
    for (auto& block : state.block_info) {
        ShaderBlock new_block{};
        new_block.start = block.start;
        new_block.end = block.end;
        new_block.branch.cond = block.branch.condition;
        new_block.branch.kills = block.branch.kill;
        new_block.branch.address = block.branch.address;
        result_out.end = std::max(result_out.end, block.end);
        result_out.blocks.push_back(new_block);
    }
    if (result_out.decompilable) {
        return true;
    }
    auto back = result_out.blocks.begin();
    auto next = std::next(back);
    while (next != result_out.blocks.end()) {
        if (state.labels.count(next->start) == 0 && next->start == back->end + 1) {
            back->end = next->end;
            next = result_out.blocks.erase(next);
            continue;
        }
        back = next;
        next++;
    }
    return true;
}
} // namespace VideoCommon::Shader
