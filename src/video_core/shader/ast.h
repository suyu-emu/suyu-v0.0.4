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
class ASTIfThen;
class ASTIfElse;
class ASTBlockEncoded;
class ASTVarSet;
class ASTGoto;
class ASTLabel;
class ASTDoWhile;
class ASTReturn;
class ASTBreak;

using ASTData = std::variant<ASTProgram, ASTIfThen, ASTIfElse, ASTBlockEncoded, ASTVarSet, ASTGoto,
                             ASTLabel, ASTDoWhile, ASTReturn, ASTBreak>;

using ASTNode = std::shared_ptr<ASTBase>;

enum class ASTZipperType : u32 {
    Program,
    IfThen,
    IfElse,
    Loop,
};

class ASTZipper final {
public:
    ASTZipper();
    ASTZipper(ASTNode first);

    ASTNode GetFirst() {
        return first;
    }

    ASTNode GetLast() {
        return last;
    }

    void PushBack(ASTNode new_node);
    void PushFront(ASTNode new_node);
    void InsertAfter(ASTNode new_node, ASTNode at_node);
    void SetParent(ASTNode new_parent);
    void DetachTail(ASTNode node);
    void DetachSingle(ASTNode node);
    void DetachSegment(ASTNode start, ASTNode end);
    void Remove(ASTNode node);

    ASTNode first{};
    ASTNode last{};
};

class ASTProgram {
public:
    ASTProgram() : nodes{} {};
    ASTZipper nodes;
};

class ASTIfThen {
public:
    ASTIfThen(Expr condition, ASTZipper nodes) : condition(condition), nodes{nodes} {}
    Expr condition;
    ASTZipper nodes;
};

class ASTIfElse {
public:
    ASTIfElse(ASTZipper nodes) : nodes{nodes} {}
    ASTZipper nodes;
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
    ASTDoWhile(Expr condition, ASTZipper nodes) : condition(condition), nodes{nodes} {}
    Expr condition;
    ASTZipper nodes;
};

class ASTReturn {
public:
    ASTReturn(Expr condition, bool kills) : condition{condition}, kills{kills} {}
    Expr condition;
    bool kills;
};

class ASTBreak {
public:
    ASTBreak(Expr condition) : condition{condition} {}
    Expr condition;
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
        auto next_parent = parent;
        while (next_parent) {
            next_parent = next_parent->GetParent();
            level++;
        }
        return level;
    }

    ASTData* GetInnerData() {
        return &data;
    }

    ASTNode GetNext() {
        return next;
    }

    ASTNode GetPrevious() {
        return previous;
    }

    ASTZipper& GetManager() {
        return *manager;
    }

    u32 GetGotoLabel() const {
        auto inner = std::get_if<ASTGoto>(&data);
        if (inner) {
            return inner->label;
        }
        return -1;
    }

    Expr GetGotoCondition() const {
        auto inner = std::get_if<ASTGoto>(&data);
        if (inner) {
            return inner->condition;
        }
        return nullptr;
    }

    void SetGotoCondition(Expr new_condition) {
        auto inner = std::get_if<ASTGoto>(&data);
        if (inner) {
            inner->condition = new_condition;
        }
    }

    bool IsIfThen() const {
        return std::holds_alternative<ASTIfThen>(data);
    }

    bool IsIfElse() const {
        return std::holds_alternative<ASTIfElse>(data);
    }

    bool IsLoop() const {
        return std::holds_alternative<ASTDoWhile>(data);
    }

    ASTZipper* GetSubNodes() {
        if (std::holds_alternative<ASTProgram>(data)) {
            return &std::get_if<ASTProgram>(&data)->nodes;
        }
        if (std::holds_alternative<ASTIfThen>(data)) {
            return &std::get_if<ASTIfThen>(&data)->nodes;
        }
        if (std::holds_alternative<ASTIfElse>(data)) {
            return &std::get_if<ASTIfElse>(&data)->nodes;
        }
        if (std::holds_alternative<ASTDoWhile>(data)) {
            return &std::get_if<ASTDoWhile>(&data)->nodes;
        }
        return nullptr;
    }

private:
    friend class ASTZipper;

    ASTData data;
    ASTNode parent;
    ASTNode next{};
    ASTNode previous{};
    ASTZipper* manager{};
};

class ASTManager final {
public:
    explicit ASTManager() {
        main_node = ASTBase::Make<ASTProgram>(ASTNode{});
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
        program->nodes.PushBack(label);
    }

    void InsertGoto(Expr condition, u32 address) {
        u32 index = labels_map[address];
        ASTNode goto_node = ASTBase::Make<ASTGoto>(main_node, condition, index);
        gotos.push_back(goto_node);
        program->nodes.PushBack(goto_node);
    }

    void InsertBlock(u32 start_address, u32 end_address) {
        ASTNode block = ASTBase::Make<ASTBlockEncoded>(main_node, start_address, end_address);
        program->nodes.PushBack(block);
    }

    void InsertReturn(Expr condition, bool kills) {
        ASTNode node = ASTBase::Make<ASTReturn>(main_node, condition, kills);
        program->nodes.PushBack(node);
    }

    std::string Print();

    void Decompile();



private:
    bool IndirectlyRelated(ASTNode first, ASTNode second);

    bool DirectlyRelated(ASTNode first, ASTNode second);

    void EncloseDoWhile(ASTNode goto_node, ASTNode label);

    void EncloseIfThen(ASTNode goto_node, ASTNode label);

    void MoveOutward(ASTNode goto_node) ;

    u32 NewVariable() {
        u32 new_var = variables;
        variables++;
        return new_var;
    }

    std::unordered_map<u32, u32> labels_map{};
    u32 labels_count{};
    std::vector<ASTNode> labels{};
    std::list<ASTNode> gotos{};
    u32 variables{};
    ASTProgram* program;
    ASTNode main_node;
};

} // namespace VideoCommon::Shader
