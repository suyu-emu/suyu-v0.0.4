// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <mutex>

#include <boost/container/small_vector.hpp>
#define BOOST_NO_MT
#include <boost/pool/detail/mutex.hpp>
#undef BOOST_NO_MT
#include <boost/icl/interval.hpp>
#include <boost/icl/interval_base_set.hpp>
#include <boost/icl/interval_set.hpp>
#include <boost/icl/split_interval_map.hpp>
#include <boost/pool/pool.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/pool/poolfwd.hpp>

#include "core/hle/service/nvdrv/core/heap_mapper.h"
#include "video_core/host1x/host1x.h"

namespace boost {
template <typename T>
class fast_pool_allocator<T, default_user_allocator_new_delete, details::pool::null_mutex, 4096, 0>;
}

namespace Service::Nvidia::NvCore {

using IntervalCompare = std::less<DAddr>;
using IntervalInstance = boost::icl::interval_type_default<DAddr, std::less>;
using IntervalAllocator = boost::fast_pool_allocator<DAddr>;
using IntervalSet = boost::icl::interval_set<DAddr>;
using IntervalType = typename IntervalSet::interval_type;

template <typename Type>
struct counter_add_functor : public boost::icl::identity_based_inplace_combine<Type> {
    // types
    typedef counter_add_functor<Type> type;
    typedef boost::icl::identity_based_inplace_combine<Type> base_type;

    // public member functions
    void operator()(Type& current, const Type& added) const {
        current += added;
        if (current < base_type::identity_element()) {
            current = base_type::identity_element();
        }
    }

    // public static functions
    static void version(Type&){};
};

using OverlapCombine = counter_add_functor<int>;
using OverlapSection = boost::icl::inter_section<int>;
using OverlapCounter = boost::icl::split_interval_map<DAddr, int>;

struct HeapMapper::HeapMapperInternal {
    HeapMapperInternal(Tegra::Host1x::Host1x& host1x) : device_memory{host1x.MemoryManager()} {}
    ~HeapMapperInternal() = default;

    template <typename Func>
    void ForEachInOverlapCounter(OverlapCounter& current_range, VAddr cpu_addr, u64 size,
                                 Func&& func) {
        const DAddr start_address = cpu_addr;
        const DAddr end_address = start_address + size;
        const IntervalType search_interval{start_address, end_address};
        auto it = current_range.lower_bound(search_interval);
        if (it == current_range.end()) {
            return;
        }
        auto end_it = current_range.upper_bound(search_interval);
        for (; it != end_it; it++) {
            auto& inter = it->first;
            DAddr inter_addr_end = inter.upper();
            DAddr inter_addr = inter.lower();
            if (inter_addr_end > end_address) {
                inter_addr_end = end_address;
            }
            if (inter_addr < start_address) {
                inter_addr = start_address;
            }
            func(inter_addr, inter_addr_end, it->second);
        }
    }

    void RemoveEachInOverlapCounter(OverlapCounter& current_range,
                                    const IntervalType search_interval, int subtract_value) {
        bool any_removals = false;
        current_range.add(std::make_pair(search_interval, subtract_value));
        do {
            any_removals = false;
            auto it = current_range.lower_bound(search_interval);
            if (it == current_range.end()) {
                return;
            }
            auto end_it = current_range.upper_bound(search_interval);
            for (; it != end_it; it++) {
                if (it->second <= 0) {
                    any_removals = true;
                    current_range.erase(it);
                    break;
                }
            }
        } while (any_removals);
    }

    IntervalSet base_set;
    OverlapCounter mapping_overlaps;
    Tegra::MaxwellDeviceMemoryManager& device_memory;
    std::mutex guard;
};

HeapMapper::HeapMapper(VAddr start_vaddress, DAddr start_daddress, size_t size, size_t smmu_id,
                       Tegra::Host1x::Host1x& host1x)
    : m_vaddress{start_vaddress}, m_daddress{start_daddress}, m_size{size}, m_smmu_id{smmu_id} {
    m_internal = std::make_unique<HeapMapperInternal>(host1x);
}

HeapMapper::~HeapMapper() {
    m_internal->device_memory.Unmap(m_daddress, m_size);
}

DAddr HeapMapper::Map(VAddr start, size_t size) {
    std::scoped_lock lk(m_internal->guard);
    m_internal->base_set.clear();
    const IntervalType interval{start, start + size};
    m_internal->base_set.insert(interval);
    m_internal->ForEachInOverlapCounter(m_internal->mapping_overlaps, start, size,
                                        [this](VAddr start_addr, VAddr end_addr, int) {
                                            const IntervalType other{start_addr, end_addr};
                                            m_internal->base_set.subtract(other);
                                        });
    if (!m_internal->base_set.empty()) {
        auto it = m_internal->base_set.begin();
        auto end_it = m_internal->base_set.end();
        for (; it != end_it; it++) {
            const VAddr inter_addr_end = it->upper();
            const VAddr inter_addr = it->lower();
            const size_t offset = inter_addr - m_vaddress;
            const size_t sub_size = inter_addr_end - inter_addr;
            m_internal->device_memory.Map(m_daddress + offset, m_vaddress + offset, sub_size,
                                          m_smmu_id);
        }
    }
    m_internal->mapping_overlaps += std::make_pair(interval, 1);
    m_internal->base_set.clear();
    return m_daddress + (start - m_vaddress);
}

void HeapMapper::Unmap(VAddr start, size_t size) {
    std::scoped_lock lk(m_internal->guard);
    m_internal->base_set.clear();
    m_internal->ForEachInOverlapCounter(m_internal->mapping_overlaps, start, size,
                                        [this](VAddr start_addr, VAddr end_addr, int value) {
                                            if (value <= 1) {
                                                const IntervalType other{start_addr, end_addr};
                                                m_internal->base_set.insert(other);
                                            }
                                        });
    if (!m_internal->base_set.empty()) {
        auto it = m_internal->base_set.begin();
        auto end_it = m_internal->base_set.end();
        for (; it != end_it; it++) {
            const VAddr inter_addr_end = it->upper();
            const VAddr inter_addr = it->lower();
            const size_t offset = inter_addr - m_vaddress;
            const size_t sub_size = inter_addr_end - inter_addr;
            m_internal->device_memory.Unmap(m_daddress + offset, sub_size);
        }
    }
    const IntervalType to_remove{start, start + size};
    m_internal->RemoveEachInOverlapCounter(m_internal->mapping_overlaps, to_remove, -1);
    m_internal->base_set.clear();
}

} // namespace Service::Nvidia::NvCore