// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2021 yuzu Emulator Project
// Copyright 2006 The Android Open Source Project
// Parts of this implementation were base on:
// https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/ui/Rect.h

#pragma once

#include <cstdint>
#include <utility>

#include "common/common_types.h"

namespace Service::android {

class Rect final {
public:
    constexpr Rect() = default;

    constexpr Rect(s32 width_, s32 height_) : right{width_}, bottom{height_} {}

    constexpr s32 Left() const {
        return left;
    }

    constexpr s32 Top() const {
        return top;
    }

    constexpr s32 Right() const {
        return right;
    }

    constexpr s32 Bottom() const {
        return bottom;
    }

    constexpr bool IsEmpty() const {
        return (GetWidth() <= 0) || (GetHeight() <= 0);
    }

    constexpr s32 GetWidth() const {
        return right - left;
    }

    constexpr s32 GetHeight() const {
        return bottom - top;
    }

    constexpr bool operator==(const Rect& rhs) const {
        return (left == rhs.left) && (top == rhs.top) && (right == rhs.right) &&
               (bottom == rhs.bottom);
    }

    constexpr bool operator!=(const Rect& rhs) const {
        return !operator==(rhs);
    }

    constexpr bool Intersect(const Rect& with, Rect* result) const {
        result->left = std::max(left, with.left);
        result->top = std::max(top, with.top);
        result->right = std::min(right, with.right);
        result->bottom = std::min(bottom, with.bottom);
        return !result->IsEmpty();
    }

private:
    s32 left{};
    s32 top{};
    s32 right{};
    s32 bottom{};
};
static_assert(sizeof(Rect) == 16, "Rect has wrong size");

} // namespace Service::android
