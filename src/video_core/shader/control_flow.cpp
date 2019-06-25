
#include <list>
#include <map>
#include <unordered_map>
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

struct ControlStack {
    std::array<u32, 20> stack;
    u32 index{};

    ControlStack() {}

    ControlStack(const ControlStack& cp) {
        index = cp.index;
        std::memcpy(stack.data(), cp.stack.data(), index * sizeof(u32));
    }

    bool Compare(const ControlStack& cs) const {
        if (index != cs.index) {
            return false;
        }
        return std::memcmp(stack.data(), cs.stack.data(), index * sizeof(u32)) == 0;
    }

    bool SoftCompare(const ControlStack& cs) const {
        if (index == 0 || cs.index == 0) {
            return index == cs.index;
        }
        return Top() == cs.Top();
    }

    u32 Size() const {
        return index;
    }

    u32 Top() const {
        return stack[index - 1];
    }

    bool Push(u32 address) {
        if (index >= 20) {
            return false;
        }
        stack[index] = address;
        index++;
        return true;
    }

    bool Pop() {
        if (index == 0) {
            return false;
        }
        index--;
        return true;
    }
};

struct Query {
    Query() {}
    Query(const Query& q) : address{q.address}, ssy_stack{q.ssy_stack}, pbk_stack{q.pbk_stack} {}
    u32 address;
    ControlStack ssy_stack{};
    ControlStack pbk_stack{};
};

struct BlockStack {
    BlockStack() = default;
    BlockStack(const BlockStack& b) : ssy_stack{b.ssy_stack}, pbk_stack{b.pbk_stack} {}
    BlockStack(const Query& q) : ssy_stack{q.ssy_stack}, pbk_stack{q.pbk_stack} {}
    ControlStack ssy_stack{};
    ControlStack pbk_stack{};
};

struct BlockBranchInfo {
    Condition condition{};
    s32 address{exit_branch};
    bool kill{};
    bool is_sync{};
    bool is_brk{};
    bool ignore{};
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

struct CFGRebuildState {
    explicit CFGRebuildState(const ProgramCode& program_code, const std::size_t program_size)
        : program_code{program_code}, program_size{program_size} {
        // queries.clear();
        block_info.clear();
        labels.clear();
        registered.clear();
        ssy_labels.clear();
        pbk_labels.clear();
        inspect_queries.clear();
        queries.clear();
    }

    std::vector<BlockInfo> block_info{};
    std::list<u32> inspect_queries{};
    std::list<Query> queries{};
    std::unordered_map<u32, u32> registered{};
    std::unordered_set<u32> labels{};
    std::map<u32, u32> ssy_labels;
    std::map<u32, u32> pbk_labels;
    std::unordered_map<u32, BlockStack> stacks{};
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
    u32 index = state.block_info.size() - 1;
    state.registered.insert({start, index});
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
            parse_info.branch_info.ignore = false;
            break;
        }
        if (state.registered.count(offset) != 0) {
            parse_info.branch_info.address = offset;
            parse_info.branch_info.ignore = true;
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
            parse_info.branch_info.ignore = false;
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
            parse_info.branch_info.ignore = false;
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
            parse_info.branch_info.ignore = false;
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
            parse_info.branch_info.ignore = false;
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
            parse_info.branch_info.ignore = false;
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
        case OpCode::Id::BRX: {
            return ParseResult::AbnormalFlow;
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
        // We need to Split the block in 2 sepparate blocks
        auto it = search_result.second;
        block_info = CreateBlockInfo(state, address, it->end);
        it->end = address - 1;
        block_info->branch = it->branch;
        BlockBranchInfo forward_branch{};
        forward_branch.address = address;
        forward_branch.ignore = true;
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

bool TryQuery(CFGRebuildState& state) {
    auto gather_labels = ([](ControlStack& cc, std::map<u32, u32>& labels, BlockInfo& block) {
        auto gather_start = labels.lower_bound(block.start);
        auto gather_end = labels.upper_bound(block.end);
        while (gather_start != gather_end) {
            cc.Push(gather_start->second);
            gather_start++;
        }
    });
    if (state.queries.empty()) {
        return false;
    }
    Query& q = state.queries.front();
    u32 block_index = state.registered[q.address];
    BlockInfo& block = state.block_info[block_index];
    // If the block is visted, check if the stacks match, else gather the ssy/pbk
    // labels into the current stack and look if the branch at the end of the block
    // consumes a label. Schedule new queries accordingly
    if (block.visited) {
        BlockStack& stack = state.stacks[q.address];
        bool all_okay = (stack.ssy_stack.Size() == 0 || q.ssy_stack.Compare(stack.ssy_stack)) &&
                        (stack.pbk_stack.Size() == 0 || q.pbk_stack.Compare(stack.pbk_stack));
        state.queries.pop_front();
        return all_okay;
    }
    block.visited = true;
    BlockStack bs{q};
    state.stacks[q.address] = bs;
    Query q2(q);
    state.queries.pop_front();
    gather_labels(q2.ssy_stack, state.ssy_labels, block);
    gather_labels(q2.pbk_stack, state.pbk_labels, block);
    if (!block.branch.condition.IsUnconditional()) {
        q2.address = block.end + 1;
        state.queries.push_back(q2);
    }
    Query conditional_query{q2};
    if (block.branch.is_sync) {
        if (block.branch.address == unassigned_branch) {
            block.branch.address = conditional_query.ssy_stack.Top();
        }
        conditional_query.ssy_stack.Pop();
    }
    if (block.branch.is_brk) {
        if (block.branch.address == unassigned_branch) {
            block.branch.address = conditional_query.pbk_stack.Top();
        }
        conditional_query.pbk_stack.Pop();
    }
    conditional_query.address = block.branch.address;
    state.queries.push_back(conditional_query);
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
    // Decompile Stacks
    Query start_query{};
    start_query.address = start_address;
    state.queries.push_back(start_query);
    bool decompiled = true;
    while (!state.queries.empty()) {
        if (!TryQuery(state)) {
            decompiled = false;
            break;
        }
    }
    // Sort and organize results
    std::sort(state.block_info.begin(), state.block_info.end(),
              [](const BlockInfo& a, const BlockInfo& b) -> bool { return a.start < b.start; });
    result_out.blocks.clear();
    result_out.decompilable = decompiled;
    result_out.start = start_address;
    result_out.end = start_address;
    for (auto& block : state.block_info) {
        ShaderBlock new_block{};
        new_block.start = block.start;
        new_block.end = block.end;
        new_block.ignore_branch = block.branch.ignore;
        if (!new_block.ignore_branch) {
            new_block.branch.cond = block.branch.condition;
            new_block.branch.kills = block.branch.kill;
            new_block.branch.address = block.branch.address;
        }
        result_out.end = std::max(result_out.end, block.end);
        result_out.blocks.push_back(new_block);
    }
    if (result_out.decompilable) {
        result_out.labels = std::move(state.labels);
        return true;
    }
    // If it's not decompilable, merge the unlabelled blocks together
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
