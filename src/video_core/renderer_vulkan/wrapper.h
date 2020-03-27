// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <exception>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "common/common_types.h"

namespace Vulkan::vk {

/**
 * Span for Vulkan arrays.
 * Based on std::span but optimized for array access instead of iterators.
 * Size returns uint32_t instead of size_t to ease interaction with Vulkan functions.
 */
template <typename T>
class Span {
public:
    /// Construct an empty span.
    constexpr Span() noexcept = default;

    /// Construct a span from a single element.
    constexpr Span(const T& value) noexcept : ptr{&value}, num{1} {}

    /// Construct a span from a range.
    template <typename Range>
    // requires std::data(const Range&)
    // requires std::size(const Range&)
    constexpr Span(const Range& range) : ptr{std::data(range)}, num{std::size(range)} {}

    /// Construct a span from a pointer and a size.
    /// This is inteded for subranges.
    constexpr Span(const T* ptr, std::size_t num) noexcept : ptr{ptr}, num{num} {}

    /// Returns the data pointer by the span.
    constexpr const T* data() const noexcept {
        return ptr;
    }

    /// Returns the number of elements in the span.
    constexpr u32 size() const noexcept {
        return static_cast<u32>(num);
    }

    /// Returns true when the span is empty.
    constexpr bool empty() const noexcept {
        return num == 0;
    }

    /// Returns a reference to the element in the passed index.
    /// @pre: index < size()
    constexpr const T& operator[](std::size_t index) const noexcept {
        return ptr[index];
    }

    /// Returns an iterator to the beginning of the span.
    constexpr const T* begin() const noexcept {
        return ptr;
    }

    /// Returns an iterator to the end of the span.
    constexpr const T* end() const noexcept {
        return ptr + num;
    }

private:
    const T* ptr = nullptr;
    std::size_t num = 0;
};

/// Converts a VkResult enum into a rodata string
const char* ToString(VkResult) noexcept;

} // namespace Vulkan::vk
