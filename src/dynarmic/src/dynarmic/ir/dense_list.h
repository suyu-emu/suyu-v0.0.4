// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <cstddef>
#include <deque>

namespace Dynarmic {
    template<typename T> struct dense_list {
        using difference_type = std::ptrdiff_t;
        using size_type = std::size_t;
        using value_type = T;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;
        using iterator = typename std::deque<value_type>::iterator;
        using const_iterator = typename std::deque<value_type>::const_iterator;
        using reverse_iterator = typename std::reverse_iterator<iterator>;
        using const_reverse_iterator = typename std::reverse_iterator<const_iterator>;

        inline bool empty() const noexcept { return list.empty(); }
        inline size_type size() const noexcept { return list.size(); }

        inline value_type& front() noexcept { return list.front(); }
        inline const value_type& front() const noexcept { return list.front(); }

        inline value_type& back() noexcept { return list.back(); }
        inline const value_type& back() const noexcept { return list.back(); }

        inline iterator begin() noexcept { return list.begin(); }
        inline const_iterator begin() const noexcept { return list.begin(); }
        inline iterator end() noexcept { return list.end(); }
        inline const_iterator end() const noexcept { return list.end(); }

        inline reverse_iterator rbegin() noexcept { return list.rbegin(); }
        inline const_reverse_iterator rbegin() const noexcept { return list.rbegin(); }
        inline reverse_iterator rend() noexcept { return list.rend(); }
        inline const_reverse_iterator rend() const noexcept { return list.rend(); }

        inline const_iterator cbegin() const noexcept { return list.cbegin(); }
        inline const_iterator cend() const noexcept { return list.cend(); }

        inline const_reverse_iterator crbegin() const noexcept { return list.crbegin(); }
        inline const_reverse_iterator crend() const noexcept { return list.crend(); }

        inline iterator insert_before(iterator it, value_type& value) noexcept {
            if (it == list.begin()) {
                list.push_front(value);
                return list.begin();
            }
            auto const index = std::distance(list.begin(), it - 1);
            list.insert(it - 1, value);
            return list.begin() + index;
        }

        std::deque<value_type> list;
    };
}
