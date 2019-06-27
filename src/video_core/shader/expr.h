// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <variant>
#include <memory>

#include "video_core/engines/shader_bytecode.h"

namespace VideoCommon::Shader {

using Tegra::Shader::ConditionCode;
using Tegra::Shader::Pred;

class ExprAnd;
class ExprOr;
class ExprNot;
class ExprPredicate;
class ExprCondCode;
class ExprVar;
class ExprBoolean;

using ExprData =
    std::variant<ExprVar, ExprCondCode, ExprPredicate, ExprNot, ExprOr, ExprAnd, ExprBoolean>;
using Expr = std::shared_ptr<ExprData>;

class ExprAnd final {
public:
    ExprAnd(Expr a, Expr b) : operand1{a}, operand2{b} {}

    Expr operand1;
    Expr operand2;
};

class ExprOr final {
public:
    ExprOr(Expr a, Expr b) : operand1{a}, operand2{b} {}

    Expr operand1;
    Expr operand2;
};

class ExprNot final {
public:
    ExprNot(Expr a) : operand1{a} {}

    Expr operand1;
};

class ExprVar final {
public:
    ExprVar(u32 index) : var_index{index} {}

    u32 var_index;
};

class ExprPredicate final {
public:
    ExprPredicate(Pred predicate) : predicate{predicate} {}

    Pred predicate;
};

class ExprCondCode final {
public:
    ExprCondCode(ConditionCode cc) : cc{cc} {}

    ConditionCode cc;
};

class ExprBoolean final {
public:
    ExprBoolean(bool val) : value{val} {}

    bool value;
};

template <typename T, typename... Args>
Expr MakeExpr(Args&&... args) {
    static_assert(std::is_convertible_v<T, ExprData>);
    return std::make_shared<ExprData>(T(std::forward<Args>(args)...));
}

} // namespace VideoCommon::Shader
