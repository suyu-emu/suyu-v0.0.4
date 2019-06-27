// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/shader/ast.h"
#include "video_core/shader/expr.h"

namespace VideoCommon::Shader {

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
        for (ASTNode& node : ast.nodes) {
            Visit(node);
        }
        inner += "}\n";
        scope--;
    }

    void operator()(ASTIf& ast) {
        ExprPrinter expr_parser{};
        std::visit(expr_parser, *ast.condition);
        inner += Ident() + "if (" + expr_parser.GetResult() + ") {\n";
        scope++;
        for (auto& node : ast.then_nodes) {
            Visit(node);
        }
        scope--;
        if (ast.else_nodes.size() > 0) {
            inner += Ident() + "} else {\n";
            scope++;
            for (auto& node : ast.else_nodes) {
                Visit(node);
            }
            scope--;
        } else {
            inner += Ident() + "}\n";
        }
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
        for (auto& node : ast.loop_nodes) {
            Visit(node);
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

} // namespace VideoCommon::Shader
