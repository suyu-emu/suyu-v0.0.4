// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
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
class ASTBlockDecoded;
class ASTBlockEncoded;
class ASTBreak;
class ASTDoWhile;
class ASTGoto;
class ASTIfElse;
class ASTIfThen;
class ASTLabel;
class ASTProgram;
class ASTReturn;
class ASTVarSet;

using ASTData = std::variant<ASTProgram, ASTIfThen, ASTIfElse, ASTBlockEncoded, ASTBlockDecoded,
                             ASTVarSet, ASTGoto, ASTLabel, ASTDoWhile, ASTReturn, ASTBreak>;

using ASTNode = std::shared_ptr<ASTBase>;

enum class ASTZipperType : u32 {
    Program,
    IfThen,
    IfElse,
    Loop,
};

class ASTZipper final {
public:
    explicit ASTZipper();

    void Init(ASTNode first, ASTNode parent);

    ASTNode GetFirst() const {
        return first;
    }

    ASTNode GetLast() const {
        return last;
    }

    void PushBack(ASTNode new_node);
    void PushFront(ASTNode new_node);
    void InsertAfter(ASTNode new_node, ASTNode at_node);
    void InsertBefore(ASTNode new_node, ASTNode at_node);
    void DetachTail(ASTNode node);
    void DetachSingle(ASTNode node);
    void DetachSegment(ASTNode start, ASTNode end);
    void Remove(ASTNode node);

    ASTNode first;
    ASTNode last;
};

class ASTProgram {
public:
    ASTZipper nodes{};
};

class ASTIfThen {
public:
    explicit ASTIfThen(Expr condition) : condition{std::move(condition)} {}
    Expr condition;
    ASTZipper nodes{};
};

class ASTIfElse {
public:
    ASTZipper nodes{};
};

class ASTBlockEncoded {
public:
    explicit ASTBlockEncoded(u32 start, u32 end) : start{start}, end{end} {}
    u32 start;
    u32 end;
};

class ASTBlockDecoded {
public:
    explicit ASTBlockDecoded(NodeBlock&& new_nodes) : nodes(std::move(new_nodes)) {}
    NodeBlock nodes;
};

class ASTVarSet {
public:
    explicit ASTVarSet(u32 index, Expr condition) : index{index}, condition{std::move(condition)} {}
    u32 index;
    Expr condition;
};

class ASTLabel {
public:
    explicit ASTLabel(u32 index) : index{index} {}
    u32 index;
    bool unused{};
};

class ASTGoto {
public:
    explicit ASTGoto(Expr condition, u32 label) : condition{std::move(condition)}, label{label} {}
    Expr condition;
    u32 label;
};

class ASTDoWhile {
public:
    explicit ASTDoWhile(Expr condition) : condition{std::move(condition)} {}
    Expr condition;
    ASTZipper nodes{};
};

class ASTReturn {
public:
    explicit ASTReturn(Expr condition, bool kills)
        : condition{std::move(condition)}, kills{kills} {}
    Expr condition;
    bool kills;
};

class ASTBreak {
public:
    explicit ASTBreak(Expr condition) : condition{std::move(condition)} {}
    Expr condition;
};

class ASTBase {
public:
    explicit ASTBase(ASTNode parent, ASTData data)
        : data{std::move(data)}, parent{std::move(parent)} {}

    template <class U, class... Args>
    static ASTNode Make(ASTNode parent, Args&&... args) {
        return std::make_shared<ASTBase>(std::move(parent),
                                         ASTData(U(std::forward<Args>(args)...)));
    }

    void SetParent(ASTNode new_parent) {
        parent = std::move(new_parent);
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

    const ASTData* GetInnerData() const {
        return &data;
    }

    ASTNode GetNext() const {
        return next;
    }

    ASTNode GetPrevious() const {
        return previous;
    }

    ASTZipper& GetManager() {
        return *manager;
    }

    const ASTZipper& GetManager() const {
        return *manager;
    }

    std::optional<u32> GetGotoLabel() const {
        auto inner = std::get_if<ASTGoto>(&data);
        if (inner) {
            return {inner->label};
        }
        return {};
    }

    Expr GetGotoCondition() const {
        auto inner = std::get_if<ASTGoto>(&data);
        if (inner) {
            return inner->condition;
        }
        return nullptr;
    }

    void MarkLabelUnused() {
        auto inner = std::get_if<ASTLabel>(&data);
        if (inner) {
            inner->unused = true;
        }
    }

    bool IsLabelUnused() const {
        auto inner = std::get_if<ASTLabel>(&data);
        if (inner) {
            return inner->unused;
        }
        return true;
    }

    std::optional<u32> GetLabelIndex() const {
        auto inner = std::get_if<ASTLabel>(&data);
        if (inner) {
            return {inner->index};
        }
        return {};
    }

    Expr GetIfCondition() const {
        auto inner = std::get_if<ASTIfThen>(&data);
        if (inner) {
            return inner->condition;
        }
        return nullptr;
    }

    void SetGotoCondition(Expr new_condition) {
        auto inner = std::get_if<ASTGoto>(&data);
        if (inner) {
            inner->condition = std::move(new_condition);
        }
    }

    bool IsIfThen() const {
        return std::holds_alternative<ASTIfThen>(data);
    }

    bool IsIfElse() const {
        return std::holds_alternative<ASTIfElse>(data);
    }

    bool IsBlockEncoded() const {
        return std::holds_alternative<ASTBlockEncoded>(data);
    }

    void TransformBlockEncoded(NodeBlock&& nodes) {
        data = ASTBlockDecoded(std::move(nodes));
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

    void Clear() {
        next.reset();
        previous.reset();
        parent.reset();
        manager = nullptr;
    }

private:
    friend class ASTZipper;

    ASTData data;
    ASTNode parent;
    ASTNode next;
    ASTNode previous;
    ASTZipper* manager{};
};

class ASTManager final {
public:
    ASTManager(bool full_decompile, bool disable_else_derivation);
    ~ASTManager();

    ASTManager(const ASTManager& o) = delete;
    ASTManager& operator=(const ASTManager& other) = delete;

    ASTManager(ASTManager&& other) noexcept = default;
    ASTManager& operator=(ASTManager&& other) noexcept = default;

    void Init();

    void DeclareLabel(u32 address);

    void InsertLabel(u32 address);

    void InsertGoto(Expr condition, u32 address);

    void InsertBlock(u32 start_address, u32 end_address);

    void InsertReturn(Expr condition, bool kills);

    std::string Print() const;

    void Decompile();

    void ShowCurrentState(std::string_view state) const;

    void SanityCheck() const;

    void Clear();

    bool IsFullyDecompiled() const {
        if (full_decompile) {
            return gotos.empty();
        }

        for (ASTNode goto_node : gotos) {
            auto label_index = goto_node->GetGotoLabel();
            if (!label_index) {
                return false;
            }
            ASTNode glabel = labels[*label_index];
            if (IsBackwardsJump(goto_node, glabel)) {
                return false;
            }
        }
        return true;
    }

    ASTNode GetProgram() const {
        return main_node;
    }

    u32 GetVariables() const {
        return variables;
    }

    const std::vector<ASTNode>& GetLabels() const {
        return labels;
    }

private:
    bool IsBackwardsJump(ASTNode goto_node, ASTNode label_node) const;

    bool IndirectlyRelated(const ASTNode& first, const ASTNode& second) const;

    bool DirectlyRelated(const ASTNode& first, const ASTNode& second) const;

    void EncloseDoWhile(ASTNode goto_node, ASTNode label);

    void EncloseIfThen(ASTNode goto_node, ASTNode label);

    void MoveOutward(ASTNode goto_node);

    u32 NewVariable() {
        return variables++;
    }

    bool full_decompile{};
    bool disable_else_derivation{};
    std::unordered_map<u32, u32> labels_map{};
    u32 labels_count{};
    std::vector<ASTNode> labels{};
    std::list<ASTNode> gotos{};
    u32 variables{};
    ASTProgram* program{};
    ASTNode main_node{};
    Expr false_condition{};
};

} // namespace VideoCommon::Shader
