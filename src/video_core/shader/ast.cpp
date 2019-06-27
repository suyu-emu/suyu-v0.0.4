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
ASTZipper::ASTZipper(ASTNode new_first) : first{}, last{} {
    first = new_first;
    last = new_first;
    ASTNode current = first;
    while (current) {
        current->manager = this;
        last = current;
        current = current->next;
    }
}

void ASTZipper::PushBack(ASTNode new_node) {
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
    new_node->previous.reset();
    new_node->next = first;
    if (first) {
        first->previous = first;
    }
    first = new_node;
    if (!last) {
        last = new_node;
    }
    new_node->manager = this;
}

void ASTZipper::InsertAfter(ASTNode new_node, ASTNode at_node) {
    if (!at_node) {
        PushFront(new_node);
        return;
    }
    new_node->previous = at_node;
    if (at_node == last) {
        last = new_node;
    }
    new_node->next = at_node->next;
    at_node->next = new_node;
    new_node->manager = this;
}

void ASTZipper::SetParent(ASTNode new_parent) {
    ASTNode current = first;
    while (current) {
        current->parent = new_parent;
        current = current->next;
    }
}

void ASTZipper::DetachTail(ASTNode node) {
    ASSERT(node->manager == this);
    if (node == first) {
        first.reset();
        last.reset();
        return;
    }

    last = node->previous;
    node->previous.reset();
}

void ASTZipper::DetachSegment(ASTNode start, ASTNode end) {
    ASSERT(start->manager == this && end->manager == this);
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
        u32 pred = static_cast<u32>(expr.predicate);
        if (pred > 7) {
            inner += "!";
            pred -= 8;
        }
        inner += "P" + std::to_string(pred);
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
        inner += Ident() + "} while (" + expr_parser.GetResult() + ")\n";
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

#pragma optimize("", off)

void ASTManager::Decompile() {
    auto it = gotos.begin();
    while (it != gotos.end()) {
        ASTNode goto_node = *it;
        u32 label_index = goto_node->GetGotoLabel();
        ASTNode label = labels[label_index];
        if (IndirectlyRelated(goto_node, label)) {
            while (!DirectlyRelated(goto_node, label)) {
                MoveOutward(goto_node);
            }
        }
        if (DirectlyRelated(goto_node, label)) {
            u32 goto_level = goto_node->GetLevel();
            u32 label_level = goto_node->GetLevel();
            while (label_level > goto_level) {
                MoveOutward(goto_node);
                goto_level++;
            }
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
    /*
    for (ASTNode label : labels) {
        auto& manager = label->GetManager();
        manager.Remove(label);
    }
    labels.clear();
    */
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

    while (min_level < max_level) {
        min_level++;
        min = min->GetParent();
    }

    return (min->GetParent() == max->GetParent());
}

void ASTManager::EncloseDoWhile(ASTNode goto_node, ASTNode label) {
    ASTZipper& zipper = goto_node->GetManager();
    ASTNode loop_start = label->GetNext();
    if (loop_start == goto_node) {
        zipper.Remove(goto_node);
        return;
    }
    ASTNode parent = label->GetParent();
    Expr condition = goto_node->GetGotoCondition();
    zipper.DetachSegment(loop_start, goto_node);
    ASTNode do_while_node = ASTBase::Make<ASTDoWhile>(parent, condition, ASTZipper(loop_start));
    zipper.InsertAfter(do_while_node, label);
    ASTZipper* sub_zipper = do_while_node->GetSubNodes();
    sub_zipper->SetParent(do_while_node);
    sub_zipper->Remove(goto_node);
}

void ASTManager::EncloseIfThen(ASTNode goto_node, ASTNode label) {
    ASTZipper& zipper = goto_node->GetManager();
    ASTNode if_end = label->GetPrevious();
    if (if_end == goto_node) {
        zipper.Remove(goto_node);
        return;
    }
    ASTNode prev = goto_node->GetPrevious();
    ASTNode parent = label->GetParent();
    Expr condition = goto_node->GetGotoCondition();
    Expr neg_condition = MakeExpr<ExprNot>(condition);
    zipper.DetachSegment(goto_node, if_end);
    ASTNode if_node = ASTBase::Make<ASTIfThen>(parent, condition, ASTZipper(goto_node));
    zipper.InsertAfter(if_node, prev);
    ASTZipper* sub_zipper = if_node->GetSubNodes();
    sub_zipper->SetParent(if_node);
    sub_zipper->Remove(goto_node);
}

void ASTManager::MoveOutward(ASTNode goto_node) {
    ASTZipper& zipper = goto_node->GetManager();
    ASTNode parent = goto_node->GetParent();
    bool is_loop = parent->IsLoop();
    bool is_if = parent->IsIfThen() || parent->IsIfElse();

    ASTNode prev = goto_node->GetPrevious();

    Expr condition = goto_node->GetGotoCondition();
    u32 var_index = NewVariable();
    Expr var_condition = MakeExpr<ExprVar>(var_index);
    ASTNode var_node = ASTBase::Make<ASTVarSet>(parent, var_index, condition);
    zipper.DetachSingle(goto_node);
    zipper.InsertAfter(var_node, prev);
    goto_node->SetGotoCondition(var_condition);
    if (is_loop) {
        ASTNode break_node = ASTBase::Make<ASTBreak>(parent, var_condition);
        zipper.InsertAfter(break_node, var_node);
    } else if (is_if) {
        ASTNode post = var_node->GetNext();
        if (post) {
            zipper.DetachTail(post);
            ASTNode if_node = ASTBase::Make<ASTIfThen>(parent, var_condition, ASTZipper(post));
            zipper.InsertAfter(if_node, var_node);
            ASTZipper* sub_zipper = if_node->GetSubNodes();
            sub_zipper->SetParent(if_node);
        }
    } else {
        UNREACHABLE();
    }
    ASTZipper& zipper2 = parent->GetManager();
    ASTNode next = parent->GetNext();
    if (is_if && next && next->IsIfElse()) {
        zipper2.InsertAfter(goto_node, next);
        return;
    }
    zipper2.InsertAfter(goto_node, parent);
}

} // namespace VideoCommon::Shader
