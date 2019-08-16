// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/shader/ast.h"
#include "video_core/shader/expr.h"

namespace VideoCommon::Shader {

ASTZipper::ASTZipper() = default;

void ASTZipper::Init(ASTNode new_first, ASTNode parent) {
    ASSERT(new_first->manager == nullptr);
    first = new_first;
    last = new_first;
    ASTNode current = first;
    while (current) {
        current->manager = this;
        current->parent = parent;
        last = current;
        current = current->next;
    }
}

void ASTZipper::PushBack(ASTNode new_node) {
    ASSERT(new_node->manager == nullptr);
    new_node->previous = last;
    if (last) {
        last->next = new_node;
    }
    new_node->next.reset();
    last = new_node;
    if (!first) {
        first = new_node;
    }
    new_node->manager = this;
}

void ASTZipper::PushFront(ASTNode new_node) {
    ASSERT(new_node->manager == nullptr);
    new_node->previous.reset();
    new_node->next = first;
    if (first) {
        first->previous = new_node;
    }
    if (last == first) {
        last = new_node;
    }
    first = new_node;
    new_node->manager = this;
}

void ASTZipper::InsertAfter(ASTNode new_node, ASTNode at_node) {
    ASSERT(new_node->manager == nullptr);
    if (!at_node) {
        PushFront(new_node);
        return;
    }
    ASTNode next = at_node->next;
    if (next) {
        next->previous = new_node;
    }
    new_node->previous = at_node;
    if (at_node == last) {
        last = new_node;
    }
    new_node->next = next;
    at_node->next = new_node;
    new_node->manager = this;
}

void ASTZipper::InsertBefore(ASTNode new_node, ASTNode at_node) {
    ASSERT(new_node->manager == nullptr);
    if (!at_node) {
        PushBack(new_node);
        return;
    }
    ASTNode previous = at_node->previous;
    if (previous) {
        previous->next = new_node;
    }
    new_node->next = at_node;
    if (at_node == first) {
        first = new_node;
    }
    new_node->previous = previous;
    at_node->previous = new_node;
    new_node->manager = this;
}

void ASTZipper::DetachTail(ASTNode node) {
    ASSERT(node->manager == this);
    if (node == first) {
        first.reset();
        last.reset();
        return;
    }

    last = node->previous;
    last->next.reset();
    node->previous.reset();
    ASTNode current = node;
    while (current) {
        current->manager = nullptr;
        current->parent.reset();
        current = current->next;
    }
}

void ASTZipper::DetachSegment(ASTNode start, ASTNode end) {
    ASSERT(start->manager == this && end->manager == this);
    if (start == end) {
        DetachSingle(start);
        return;
    }
    ASTNode prev = start->previous;
    ASTNode post = end->next;
    if (!prev) {
        first = post;
    } else {
        prev->next = post;
    }
    if (!post) {
        last = prev;
    } else {
        post->previous = prev;
    }
    start->previous.reset();
    end->next.reset();
    ASTNode current = start;
    bool found = false;
    while (current) {
        current->manager = nullptr;
        current->parent.reset();
        found |= current == end;
        current = current->next;
    }
    ASSERT(found);
}

void ASTZipper::DetachSingle(ASTNode node) {
    ASSERT(node->manager == this);
    ASTNode prev = node->previous;
    ASTNode post = node->next;
    node->previous.reset();
    node->next.reset();
    if (!prev) {
        first = post;
    } else {
        prev->next = post;
    }
    if (!post) {
        last = prev;
    } else {
        post->previous = prev;
    }

    node->manager = nullptr;
    node->parent.reset();
}

void ASTZipper::Remove(ASTNode node) {
    ASSERT(node->manager == this);
    ASTNode next = node->next;
    ASTNode previous = node->previous;
    if (previous) {
        previous->next = next;
    }
    if (next) {
        next->previous = previous;
    }
    node->parent.reset();
    node->manager = nullptr;
    if (node == last) {
        last = previous;
    }
    if (node == first) {
        first = next;
    }
}

class ExprPrinter final {
public:
    ExprPrinter() = default;

    void operator()(ExprAnd const& expr) {
        inner += "( ";
        std::visit(*this, *expr.operand1);
        inner += " && ";
        std::visit(*this, *expr.operand2);
        inner += ')';
    }

    void operator()(ExprOr const& expr) {
        inner += "( ";
        std::visit(*this, *expr.operand1);
        inner += " || ";
        std::visit(*this, *expr.operand2);
        inner += ')';
    }

    void operator()(ExprNot const& expr) {
        inner += "!";
        std::visit(*this, *expr.operand1);
    }

    void operator()(ExprPredicate const& expr) {
        inner += "P" + std::to_string(expr.predicate);
    }

    void operator()(ExprCondCode const& expr) {
        u32 cc = static_cast<u32>(expr.cc);
        inner += "CC" + std::to_string(cc);
    }

    void operator()(ExprVar const& expr) {
        inner += "V" + std::to_string(expr.var_index);
    }

    void operator()(ExprBoolean const& expr) {
        inner += expr.value ? "true" : "false";
    }

    std::string& GetResult() {
        return inner;
    }

    std::string inner{};
};

class ASTPrinter {
public:
    ASTPrinter() = default;

    void operator()(ASTProgram& ast) {
        scope++;
        inner += "program {\n";
        ASTNode current = ast.nodes.GetFirst();
        while (current) {
            Visit(current);
            current = current->GetNext();
        }
        inner += "}\n";
        scope--;
    }

    void operator()(ASTIfThen& ast) {
        ExprPrinter expr_parser{};
        std::visit(expr_parser, *ast.condition);
        inner += Ident() + "if (" + expr_parser.GetResult() + ") {\n";
        scope++;
        ASTNode current = ast.nodes.GetFirst();
        while (current) {
            Visit(current);
            current = current->GetNext();
        }
        scope--;
        inner += Ident() + "}\n";
    }

    void operator()(ASTIfElse& ast) {
        inner += Ident() + "else {\n";
        scope++;
        ASTNode current = ast.nodes.GetFirst();
        while (current) {
            Visit(current);
            current = current->GetNext();
        }
        scope--;
        inner += Ident() + "}\n";
    }

    void operator()(ASTBlockEncoded& ast) {
        inner += Ident() + "Block(" + std::to_string(ast.start) + ", " + std::to_string(ast.end) +
                 ");\n";
    }

    void operator()(ASTBlockDecoded& ast) {
        inner += Ident() + "Block;\n";
    }

    void operator()(ASTVarSet& ast) {
        ExprPrinter expr_parser{};
        std::visit(expr_parser, *ast.condition);
        inner +=
            Ident() + "V" + std::to_string(ast.index) + " := " + expr_parser.GetResult() + ";\n";
    }

    void operator()(ASTLabel& ast) {
        inner += "Label_" + std::to_string(ast.index) + ":\n";
    }

    void operator()(ASTGoto& ast) {
        ExprPrinter expr_parser{};
        std::visit(expr_parser, *ast.condition);
        inner += Ident() + "(" + expr_parser.GetResult() + ") -> goto Label_" +
                 std::to_string(ast.label) + ";\n";
    }

    void operator()(ASTDoWhile& ast) {
        ExprPrinter expr_parser{};
        std::visit(expr_parser, *ast.condition);
        inner += Ident() + "do {\n";
        scope++;
        ASTNode current = ast.nodes.GetFirst();
        while (current) {
            Visit(current);
            current = current->GetNext();
        }
        scope--;
        inner += Ident() + "} while (" + expr_parser.GetResult() + ");\n";
    }

    void operator()(ASTReturn& ast) {
        ExprPrinter expr_parser{};
        std::visit(expr_parser, *ast.condition);
        inner += Ident() + "(" + expr_parser.GetResult() + ") -> " +
                 (ast.kills ? "discard" : "exit") + ";\n";
    }

    void operator()(ASTBreak& ast) {
        ExprPrinter expr_parser{};
        std::visit(expr_parser, *ast.condition);
        inner += Ident() + "(" + expr_parser.GetResult() + ") -> break;\n";
    }

    std::string& Ident() {
        if (memo_scope == scope) {
            return tabs_memo;
        }
        tabs_memo = tabs.substr(0, scope * 2);
        memo_scope = scope;
        return tabs_memo;
    }

    void Visit(ASTNode& node) {
        std::visit(*this, *node->GetInnerData());
    }

    std::string& GetResult() {
        return inner;
    }

private:
    std::string inner{};
    u32 scope{};

    std::string tabs_memo{};
    u32 memo_scope{};

    static std::string tabs;
};

std::string ASTPrinter::tabs = "                                    ";

std::string ASTManager::Print() {
    ASTPrinter printer{};
    printer.Visit(main_node);
    return printer.GetResult();
}

ASTManager::ASTManager(bool full_decompile) : full_decompile{full_decompile} {};

ASTManager::~ASTManager() {
    Clear();
}

void ASTManager::Init() {
    main_node = ASTBase::Make<ASTProgram>(ASTNode{});
    program = std::get_if<ASTProgram>(main_node->GetInnerData());
    false_condition = MakeExpr<ExprBoolean>(false);
}

ASTManager::ASTManager(ASTManager&& other)
    : labels_map(std::move(other.labels_map)), labels_count{other.labels_count},
      gotos(std::move(other.gotos)), labels(std::move(other.labels)), variables{other.variables},
      program{other.program}, main_node{other.main_node}, false_condition{other.false_condition} {
    other.main_node.reset();
}

ASTManager& ASTManager::operator=(ASTManager&& other) {
    full_decompile = other.full_decompile;
    labels_map = std::move(other.labels_map);
    labels_count = other.labels_count;
    gotos = std::move(other.gotos);
    labels = std::move(other.labels);
    variables = other.variables;
    program = other.program;
    main_node = other.main_node;
    false_condition = other.false_condition;

    other.main_node.reset();
    return *this;
}

void ASTManager::DeclareLabel(u32 address) {
    const auto pair = labels_map.emplace(address, labels_count);
    if (pair.second) {
        labels_count++;
        labels.resize(labels_count);
    }
}

void ASTManager::InsertLabel(u32 address) {
    u32 index = labels_map[address];
    ASTNode label = ASTBase::Make<ASTLabel>(main_node, index);
    labels[index] = label;
    program->nodes.PushBack(label);
}

void ASTManager::InsertGoto(Expr condition, u32 address) {
    u32 index = labels_map[address];
    ASTNode goto_node = ASTBase::Make<ASTGoto>(main_node, condition, index);
    gotos.push_back(goto_node);
    program->nodes.PushBack(goto_node);
}

void ASTManager::InsertBlock(u32 start_address, u32 end_address) {
    ASTNode block = ASTBase::Make<ASTBlockEncoded>(main_node, start_address, end_address);
    program->nodes.PushBack(block);
}

void ASTManager::InsertReturn(Expr condition, bool kills) {
    ASTNode node = ASTBase::Make<ASTReturn>(main_node, condition, kills);
    program->nodes.PushBack(node);
}

void ASTManager::Decompile() {
    auto it = gotos.begin();
    while (it != gotos.end()) {
        ASTNode goto_node = *it;
        u32 label_index = goto_node->GetGotoLabel();
        ASTNode label = labels[label_index];
        if (!full_decompile) {
            // We only decompile backward jumps
            if (!IsBackwardsJump(goto_node, label)) {
                it++;
                continue;
            }
        }
        if (IndirectlyRelated(goto_node, label)) {
            while (!DirectlyRelated(goto_node, label)) {
                MoveOutward(goto_node);
            }
        }
        if (DirectlyRelated(goto_node, label)) {
            u32 goto_level = goto_node->GetLevel();
            u32 label_level = label->GetLevel();
            while (label_level < goto_level) {
                MoveOutward(goto_node);
                goto_level--;
            }
            // TODO(Blinkhawk): Implement Lifting and Inward Movements
        }
        if (label->GetParent() == goto_node->GetParent()) {
            bool is_loop = false;
            ASTNode current = goto_node->GetPrevious();
            while (current) {
                if (current == label) {
                    is_loop = true;
                    break;
                }
                current = current->GetPrevious();
            }

            if (is_loop) {
                EncloseDoWhile(goto_node, label);
            } else {
                EncloseIfThen(goto_node, label);
            }
            it = gotos.erase(it);
            continue;
        }
        it++;
    }
    if (full_decompile) {
        for (ASTNode label : labels) {
            auto& manager = label->GetManager();
            manager.Remove(label);
        }
        labels.clear();
    } else {
        auto it = labels.begin();
        while (it != labels.end()) {
            bool can_remove = true;
            ASTNode label = *it;
            for (ASTNode goto_node : gotos) {
                u32 label_index = goto_node->GetGotoLabel();
                ASTNode glabel = labels[label_index];
                if (glabel == label) {
                    can_remove = false;
                    break;
                }
            }
            if (can_remove) {
                auto& manager = label->GetManager();
                manager.Remove(label);
                labels.erase(it);
            }
        }
    }
}

bool ASTManager::IsBackwardsJump(ASTNode goto_node, ASTNode label_node) const {
    u32 goto_level = goto_node->GetLevel();
    u32 label_level = label_node->GetLevel();
    while (goto_level > label_level) {
        goto_level--;
        goto_node = goto_node->GetParent();
    }
    while (label_level > goto_level) {
        label_level--;
        label_node = label_node->GetParent();
    }
    while (goto_node->GetParent() != label_node->GetParent()) {
        goto_node = goto_node->GetParent();
        label_node = label_node->GetParent();
    }
    ASTNode current = goto_node->GetPrevious();
    while (current) {
        if (current == label_node) {
            return true;
        }
        current = current->GetPrevious();
    }
    return false;
}

ASTNode CommonParent(ASTNode first, ASTNode second) {
    if (first->GetParent() == second->GetParent()) {
        return first->GetParent();
    }
    u32 first_level = first->GetLevel();
    u32 second_level = second->GetLevel();
    u32 min_level;
    u32 max_level;
    ASTNode max;
    ASTNode min;
    if (first_level > second_level) {
        min_level = second_level;
        min = second;
        max_level = first_level;
        max = first;
    } else {
        min_level = first_level;
        min = first;
        max_level = second_level;
        max = second;
    }

    while (max_level > min_level) {
        max_level--;
        max = max->GetParent();
    }

    while (min->GetParent() != max->GetParent()) {
        min = min->GetParent();
        max = max->GetParent();
    }
    return min->GetParent();
}

bool ASTManager::IndirectlyRelated(ASTNode first, ASTNode second) {
    return !(first->GetParent() == second->GetParent() || DirectlyRelated(first, second));
}

bool ASTManager::DirectlyRelated(ASTNode first, ASTNode second) {
    if (first->GetParent() == second->GetParent()) {
        return false;
    }
    u32 first_level = first->GetLevel();
    u32 second_level = second->GetLevel();
    u32 min_level;
    u32 max_level;
    ASTNode max;
    ASTNode min;
    if (first_level > second_level) {
        min_level = second_level;
        min = second;
        max_level = first_level;
        max = first;
    } else {
        min_level = first_level;
        min = first;
        max_level = second_level;
        max = second;
    }

    while (max_level > min_level) {
        max_level--;
        max = max->GetParent();
    }

    return (min->GetParent() == max->GetParent());
}

void ASTManager::ShowCurrentState(std::string state) {
    LOG_CRITICAL(HW_GPU, "\nState {}:\n\n{}\n", state, Print());
    SanityCheck();
}

void ASTManager::SanityCheck() {
    for (auto label : labels) {
        if (!label->GetParent()) {
            LOG_CRITICAL(HW_GPU, "Sanity Check Failed");
        }
    }
}

void ASTManager::EncloseDoWhile(ASTNode goto_node, ASTNode label) {
    // ShowCurrentState("Before DoWhile Enclose");
    ASTZipper& zipper = goto_node->GetManager();
    ASTNode loop_start = label->GetNext();
    if (loop_start == goto_node) {
        zipper.Remove(goto_node);
        // ShowCurrentState("Ignore DoWhile Enclose");
        return;
    }
    ASTNode parent = label->GetParent();
    Expr condition = goto_node->GetGotoCondition();
    zipper.DetachSegment(loop_start, goto_node);
    ASTNode do_while_node = ASTBase::Make<ASTDoWhile>(parent, condition);
    ASTZipper* sub_zipper = do_while_node->GetSubNodes();
    sub_zipper->Init(loop_start, do_while_node);
    zipper.InsertAfter(do_while_node, label);
    sub_zipper->Remove(goto_node);
    // ShowCurrentState("After DoWhile Enclose");
}

void ASTManager::EncloseIfThen(ASTNode goto_node, ASTNode label) {
    // ShowCurrentState("Before IfThen Enclose");
    ASTZipper& zipper = goto_node->GetManager();
    ASTNode if_end = label->GetPrevious();
    if (if_end == goto_node) {
        zipper.Remove(goto_node);
        // ShowCurrentState("Ignore IfThen Enclose");
        return;
    }
    ASTNode prev = goto_node->GetPrevious();
    Expr condition = goto_node->GetGotoCondition();
    bool do_else = false;
    if (prev->IsIfThen()) {
        Expr if_condition = prev->GetIfCondition();
        do_else = ExprAreEqual(if_condition, condition);
    }
    ASTNode parent = label->GetParent();
    zipper.DetachSegment(goto_node, if_end);
    ASTNode if_node;
    if (do_else) {
        if_node = ASTBase::Make<ASTIfElse>(parent);
    } else {
        Expr neg_condition = MakeExprNot(condition);
        if_node = ASTBase::Make<ASTIfThen>(parent, neg_condition);
    }
    ASTZipper* sub_zipper = if_node->GetSubNodes();
    sub_zipper->Init(goto_node, if_node);
    zipper.InsertAfter(if_node, prev);
    sub_zipper->Remove(goto_node);
    // ShowCurrentState("After IfThen Enclose");
}

void ASTManager::MoveOutward(ASTNode goto_node) {
    // ShowCurrentState("Before MoveOutward");
    ASTZipper& zipper = goto_node->GetManager();
    ASTNode parent = goto_node->GetParent();
    ASTZipper& zipper2 = parent->GetManager();
    ASTNode grandpa = parent->GetParent();
    bool is_loop = parent->IsLoop();
    bool is_else = parent->IsIfElse();
    bool is_if = parent->IsIfThen();

    ASTNode prev = goto_node->GetPrevious();
    ASTNode post = goto_node->GetNext();

    Expr condition = goto_node->GetGotoCondition();
    zipper.DetachSingle(goto_node);
    if (is_loop) {
        u32 var_index = NewVariable();
        Expr var_condition = MakeExpr<ExprVar>(var_index);
        ASTNode var_node = ASTBase::Make<ASTVarSet>(parent, var_index, condition);
        ASTNode var_node_init = ASTBase::Make<ASTVarSet>(parent, var_index, false_condition);
        zipper2.InsertBefore(var_node_init, parent);
        zipper.InsertAfter(var_node, prev);
        goto_node->SetGotoCondition(var_condition);
        ASTNode break_node = ASTBase::Make<ASTBreak>(parent, var_condition);
        zipper.InsertAfter(break_node, var_node);
    } else if (is_if || is_else) {
        if (post) {
            u32 var_index = NewVariable();
            Expr var_condition = MakeExpr<ExprVar>(var_index);
            ASTNode var_node = ASTBase::Make<ASTVarSet>(parent, var_index, condition);
            ASTNode var_node_init = ASTBase::Make<ASTVarSet>(parent, var_index, false_condition);
            if (is_if) {
                zipper2.InsertBefore(var_node_init, parent);
            } else {
                zipper2.InsertBefore(var_node_init, parent->GetPrevious());
            }
            zipper.InsertAfter(var_node, prev);
            goto_node->SetGotoCondition(var_condition);
            zipper.DetachTail(post);
            ASTNode if_node = ASTBase::Make<ASTIfThen>(parent, MakeExprNot(var_condition));
            ASTZipper* sub_zipper = if_node->GetSubNodes();
            sub_zipper->Init(post, if_node);
            zipper.InsertAfter(if_node, var_node);
        } else {
            Expr if_condition;
            if (is_if) {
                if_condition = parent->GetIfCondition();
            } else {
                ASTNode if_node = parent->GetPrevious();
                if_condition = MakeExprNot(if_node->GetIfCondition());
            }
            Expr new_condition = MakeExprAnd(if_condition, condition);
            goto_node->SetGotoCondition(new_condition);
        }
    } else {
        UNREACHABLE();
    }
    ASTNode next = parent->GetNext();
    if (is_if && next && next->IsIfElse()) {
        zipper2.InsertAfter(goto_node, next);
        goto_node->SetParent(grandpa);
        // ShowCurrentState("After MoveOutward");
        return;
    }
    zipper2.InsertAfter(goto_node, parent);
    goto_node->SetParent(grandpa);
    // ShowCurrentState("After MoveOutward");
}

class ASTClearer {
public:
    ASTClearer() = default;

    void operator()(ASTProgram& ast) {
        ASTNode current = ast.nodes.GetFirst();
        while (current) {
            Visit(current);
            current = current->GetNext();
        }
    }

    void operator()(ASTIfThen& ast) {
        ASTNode current = ast.nodes.GetFirst();
        while (current) {
            Visit(current);
            current = current->GetNext();
        }
    }

    void operator()(ASTIfElse& ast) {
        ASTNode current = ast.nodes.GetFirst();
        while (current) {
            Visit(current);
            current = current->GetNext();
        }
    }

    void operator()(ASTBlockEncoded& ast) {}

    void operator()(ASTBlockDecoded& ast) {
        ast.nodes.clear();
    }

    void operator()(ASTVarSet& ast) {}

    void operator()(ASTLabel& ast) {}

    void operator()(ASTGoto& ast) {}

    void operator()(ASTDoWhile& ast) {
        ASTNode current = ast.nodes.GetFirst();
        while (current) {
            Visit(current);
            current = current->GetNext();
        }
    }

    void operator()(ASTReturn& ast) {}

    void operator()(ASTBreak& ast) {}

    void Visit(ASTNode& node) {
        std::visit(*this, *node->GetInnerData());
        node->Clear();
    }
};

void ASTManager::Clear() {
    if (!main_node) {
        return;
    }
    ASTClearer clearer{};
    clearer.Visit(main_node);
    main_node.reset();
    program = nullptr;
    labels_map.clear();
    labels.clear();
    gotos.clear();
}

} // namespace VideoCommon::Shader
