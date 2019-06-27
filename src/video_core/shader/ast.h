// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "video_core/shader/expr.h"
#include "video_core/shader/node.h"

namespace VideoCommon::Shader {

class ASTBase;
class ASTProgram;
class ASTIf;
class ASTBlockEncoded;
class ASTVarSet;
class ASTGoto;
class ASTLabel;
class ASTDoWhile;
class ASTReturn;

using ASTData = std::variant<ASTProgram, ASTIf, ASTBlockEncoded, ASTVarSet, ASTGoto, ASTLabel,
                             ASTDoWhile, ASTReturn>;

using ASTNode = std::shared_ptr<ASTBase>;

class ASTProgram {
public:
    ASTProgram() = default;
    std::list<ASTNode> nodes;
};

class ASTIf {
public:
    ASTIf(Expr condition, std::list<ASTNode> then_nodes, std::list<ASTNode> else_nodes)
        : condition(condition), then_nodes{then_nodes}, else_nodes{then_nodes} {}
    Expr condition;
    std::list<ASTNode> then_nodes;
    std::list<ASTNode> else_nodes;
};

class ASTBlockEncoded {
public:
    ASTBlockEncoded(u32 start, u32 end) : start{start}, end{end} {}
    u32 start;
    u32 end;
};

class ASTVarSet {
public:
    ASTVarSet(u32 index, Expr condition) : index{index}, condition{condition} {}
    u32 index;
    Expr condition;
};

class ASTLabel {
public:
    ASTLabel(u32 index) : index{index} {}
    u32 index;
};

class ASTGoto {
public:
    ASTGoto(Expr condition, u32 label) : condition{condition}, label{label} {}
    Expr condition;
    u32 label;
};

class ASTDoWhile {
public:
    ASTDoWhile(Expr condition, std::list<ASTNode> loop_nodes)
        : condition(condition), loop_nodes{loop_nodes} {}
    Expr condition;
    std::list<ASTNode> loop_nodes;
};

class ASTReturn {
public:
    ASTReturn(Expr condition, bool kills) : condition{condition}, kills{kills} {}
    Expr condition;
    bool kills;
};

class ASTBase {
public:
    explicit ASTBase(ASTNode parent, ASTData data) : parent{parent}, data{data} {}

    template <class U, class... Args>
    static ASTNode Make(ASTNode parent, Args&&... args) {
        return std::make_shared<ASTBase>(parent, ASTData(U(std::forward<Args>(args)...)));
    }

    void SetParent(ASTNode new_parent) {
        parent = new_parent;
    }

    ASTNode& GetParent() {
        return parent;
    }

    const ASTNode& GetParent() const {
        return parent;
    }

    u32 GetLevel() const {
        u32 level = 0;
        auto next = parent;
        while (next) {
            next = next->GetParent();
            level++;
        }
        return level;
    }

    ASTData* GetInnerData() {
        return &data;
    }

private:
    ASTData data;
    ASTNode parent;
};

class ASTManager final {
public:
    explicit ASTManager() {
        main_node = ASTBase::Make<ASTProgram>(nullptr);
        program = std::get_if<ASTProgram>(main_node->GetInnerData());
    }

    void DeclareLabel(u32 address) {
        const auto pair = labels_map.emplace(address, labels_count);
        if (pair.second) {
            labels_count++;
            labels.resize(labels_count);
        }
    }

    void InsertLabel(u32 address) {
        u32 index = labels_map[address];
        ASTNode label = ASTBase::Make<ASTLabel>(main_node, index);
        labels[index] = label;
        program->nodes.push_back(label);
    }

    void InsertGoto(Expr condition, u32 address) {
        u32 index = labels_map[address];
        ASTNode goto_node = ASTBase::Make<ASTGoto>(main_node, condition, index);
        gotos.push_back(goto_node);
        program->nodes.push_back(goto_node);
    }

    void InsertBlock(u32 start_address, u32 end_address) {
        ASTNode block = ASTBase::Make<ASTBlockEncoded>(main_node, start_address, end_address);
        program->nodes.push_back(block);
    }

    void InsertReturn(Expr condition, bool kills) {
        ASTNode node = ASTBase::Make<ASTReturn>(main_node, condition, kills);
        program->nodes.push_back(node);
    }

    std::string Print();

    void Decompile() {}

private:
    std::unordered_map<u32, u32> labels_map{};
    u32 labels_count{};
    std::vector<ASTNode> labels{};
    std::list<ASTNode> gotos{};
    u32 variables{};
    ASTProgram* program;
    ASTNode main_node;
};

} // namespace VideoCommon::Shader
