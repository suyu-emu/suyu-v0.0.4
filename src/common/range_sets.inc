// SPDX-FileCopyrightText: 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <limits>
#include <utility>

#define BOOST_NO_MT
#include <boost/pool/detail/mutex.hpp>
#undef BOOST_NO_MT
#include <boost/icl/interval.hpp>
#include <boost/icl/interval_base_set.hpp>
#include <boost/icl/interval_map.hpp>
#include <boost/icl/interval_set.hpp>
#include <boost/icl/split_interval_map.hpp>
#include <boost/pool/pool.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/pool/poolfwd.hpp>

#include "common/range_sets.h"

namespace boost {
template <typename T>
class fast_pool_allocator<T, default_user_allocator_new_delete, details::pool::null_mutex, 4096, 0>;
}

namespace Common {

template <typename AddressType>
struct RangeSet<AddressType>::RangeSetImpl {
    using IntervalSet = boost::icl::interval_set<
        AddressType, std::less, ICL_INTERVAL_INSTANCE(ICL_INTERVAL_DEFAULT, AddressType, std::less),
        boost::fast_pool_allocator>;
    using IntervalType = typename IntervalSet::interval_type;

    RangeSetImpl() = default;
    ~RangeSetImpl() = default;

    void Add(AddressType base_address, size_t size) {
        AddressType end_address = base_address + static_cast<AddressType>(size);
        IntervalType interval{base_address, end_address};
        m_ranges_set.add(interval);
    }

    void Subtract(AddressType base_address, size_t size) {
        AddressType end_address = base_address + static_cast<AddressType>(size);
        IntervalType interval{base_address, end_address};
        m_ranges_set.subtract(interval);
    }

    IntervalSet m_ranges_set;
};

template <typename AddressType>
struct SplitRangeSet<AddressType>::SplitRangeSetImpl {

    using IntervalSet =
        boost::icl::split_interval_map<AddressType, s32, boost::icl::partial_enricher, std::less,
                                       boost::icl::inplace_plus, boost::icl::inter_section,
                                       ICL_INTERVAL_INSTANCE(ICL_INTERVAL_DEFAULT, AddressType,
                                                             std::less),
                                       boost::fast_pool_allocator>;
    using IntervalType = typename IntervalSet::interval_type;

    SplitRangeSetImpl() = default;
    ~SplitRangeSetImpl() = default;

    void Add(AddressType base_address, size_t size) {
        AddressType end_address = base_address + static_cast<AddressType>(size);
        IntervalType interval{base_address, end_address};
        m_split_ranges_set += std::make_pair(interval, 1);
    }

    template <bool has_on_delete, typename Func>
    void Subtract(AddressType base_address, size_t size, s32 amount,
                  [[maybe_unused]] Func&& on_delete) {
        AddressType end_address = base_address + static_cast<AddressType>(size);
        IntervalType interval{base_address, end_address};
        bool any_removals = false;
        m_split_ranges_set += std::make_pair(interval, -amount);
        do {
            any_removals = false;
            auto it = m_split_ranges_set.lower_bound(interval);
            if (it == m_split_ranges_set.end()) {
                return;
            }
            auto end_it = m_split_ranges_set.upper_bound(interval);
            for (; it != end_it; it++) {
                if (it->second <= 0) {
                    if constexpr (has_on_delete) {
                        if (it->second == 0) {
                            on_delete(it->first.lower(), it->first.upper());
                        }
                    }
                    any_removals = true;
                    m_split_ranges_set.erase(it);
                    break;
                }
            }
        } while (any_removals);
    }

    IntervalSet m_split_ranges_set;
};

template <typename AddressType>
RangeSet<AddressType>::RangeSet() {
    m_impl = std::make_unique<RangeSet<AddressType>::RangeSetImpl>();
}

template <typename AddressType>
RangeSet<AddressType>::~RangeSet() = default;

template <typename AddressType>
RangeSet<AddressType>::RangeSet(RangeSet&& other) {
    m_impl = std::make_unique<RangeSet<AddressType>::RangeSetImpl>();
    m_impl->m_ranges_set = std::move(other.m_impl->m_ranges_set);
}

template <typename AddressType>
RangeSet<AddressType>& RangeSet<AddressType>::operator=(RangeSet&& other) {
    m_impl->m_ranges_set = std::move(other.m_impl->m_ranges_set);
}

template <typename AddressType>
void RangeSet<AddressType>::Add(AddressType base_address, size_t size) {
    m_impl->Add(base_address, size);
}

template <typename AddressType>
void RangeSet<AddressType>::Subtract(AddressType base_address, size_t size) {
    m_impl->Subtract(base_address, size);
}

template <typename AddressType>
void RangeSet<AddressType>::Clear() {
    m_impl->m_ranges_set.clear();
}

template <typename AddressType>
bool RangeSet<AddressType>::Empty() const {
    return m_impl->m_ranges_set.empty();
}

template <typename AddressType>
template <typename Func>
void RangeSet<AddressType>::ForEach(Func&& func) const {
    if (m_impl->m_ranges_set.empty()) {
        return;
    }
    auto it = m_impl->m_ranges_set.begin();
    auto end_it = m_impl->m_ranges_set.end();
    for (; it != end_it; it++) {
        const AddressType inter_addr_end = it->upper();
        const AddressType inter_addr = it->lower();
        func(inter_addr, inter_addr_end);
    }
}

template <typename AddressType>
template <typename Func>
void RangeSet<AddressType>::ForEachInRange(AddressType base_addr, size_t size, Func&& func) const {
    auto& range_set = m_impl->m_ranges_set;
    const AddressType start_address = base_addr;
    const AddressType end_address = start_address + size;
    const RangeSetImpl::IntervalType search_interval{start_address, end_address};
    auto it = range_set.lower_bound(search_interval);
    if (it == range_set.end()) {
        return;
    }
    auto end_it = range_set.upper_bound(search_interval);
    for (; it != end_it; it++) {
        AddressType inter_addr_end = it->upper();
        AddressType inter_addr = it->lower();
        if (inter_addr_end > end_address) {
            inter_addr_end = end_address;
        }
        if (inter_addr < start_address) {
            inter_addr = start_address;
        }
        func(inter_addr, inter_addr_end);
    }
}

template <typename AddressType>
SplitRangeSet<AddressType>::SplitRangeSet() {
    m_impl = std::make_unique<SplitRangeSet<AddressType>::SplitRangeSetImpl>();
}

template <typename AddressType>
SplitRangeSet<AddressType>::~SplitRangeSet() = default;

template <typename AddressType>
SplitRangeSet<AddressType>::SplitRangeSet(SplitRangeSet&& other) {
    m_impl = std::make_unique<SplitRangeSet<AddressType>::SplitRangeSetImpl>();
    m_impl->m_split_ranges_set = std::move(other.m_impl->m_split_ranges_set);
}

template <typename AddressType>
SplitRangeSet<AddressType>& SplitRangeSet<AddressType>::operator=(SplitRangeSet&& other) {
    m_impl->m_split_ranges_set = std::move(other.m_impl->m_split_ranges_set);
}

template <typename AddressType>
void SplitRangeSet<AddressType>::Add(AddressType base_address, size_t size) {
    m_impl->Add(base_address, size);
}

template <typename AddressType>
void SplitRangeSet<AddressType>::Subtract(AddressType base_address, size_t size) {
    m_impl->Subtract<false>(base_address, size, 1, [](AddressType, AddressType) {});
}

template <typename AddressType>
template <typename Func>
void SplitRangeSet<AddressType>::Subtract(AddressType base_address, size_t size, Func&& on_delete) {
    m_impl->Subtract<true>(base_address, size, 1, on_delete);
}

template <typename AddressType>
void SplitRangeSet<AddressType>::DeleteAll(AddressType base_address, size_t size) {
    m_impl->Subtract<false>(base_address, size, std::numeric_limits<s32>::max(),
                            [](AddressType, AddressType) {});
}

template <typename AddressType>
void SplitRangeSet<AddressType>::Clear() {
    m_impl->m_split_ranges_set.clear();
}

template <typename AddressType>
bool SplitRangeSet<AddressType>::Empty() const {
    return m_impl->m_split_ranges_set.empty();
}

template <typename AddressType>
template <typename Func>
void SplitRangeSet<AddressType>::ForEach(Func&& func) const {
    if (m_impl->m_split_ranges_set.empty()) {
        return;
    }
    auto it = m_impl->m_split_ranges_set.begin();
    auto end_it = m_impl->m_split_ranges_set.end();
    for (; it != end_it; it++) {
        const AddressType inter_addr_end = it->first.upper();
        const AddressType inter_addr = it->first.lower();
        func(inter_addr, inter_addr_end, it->second);
    }
}

template <typename AddressType>
template <typename Func>
void SplitRangeSet<AddressType>::ForEachInRange(AddressType base_address, size_t size,
                                                Func&& func) const {
    auto& range_set = m_impl->m_split_ranges_set;
    const AddressType start_address = base_address;
    const AddressType end_address = start_address + size;
    const SplitRangeSetImpl::IntervalType search_interval{start_address, end_address};
    auto it = range_set.lower_bound(search_interval);
    if (it == range_set.end()) {
        return;
    }
    auto end_it = range_set.upper_bound(search_interval);
    for (; it != end_it; it++) {
        auto& inter = it->first;
        AddressType inter_addr_end = inter.upper();
        AddressType inter_addr = inter.lower();
        if (inter_addr_end > end_address) {
            inter_addr_end = end_address;
        }
        if (inter_addr < start_address) {
            inter_addr = start_address;
        }
        func(inter_addr, inter_addr_end, it->second);
    }
}

} // namespace Common