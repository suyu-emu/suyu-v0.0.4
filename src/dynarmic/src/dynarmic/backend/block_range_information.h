/* This file is part of the dynarmic project.
 * Copyright (c) 2018 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <set>

#include <boost/icl/interval_map.hpp>
#include <boost/icl/interval_set.hpp>
#include <ankerl/unordered_dense.h>

#include "dynarmic/ir/location_descriptor.h"

namespace Dynarmic::Backend {

template<typename ProgramCounterType>
class BlockRangeInformation {
public:
    void AddRange(boost::icl::discrete_interval<ProgramCounterType> range, IR::LocationDescriptor location);
    void ClearCache();
    ankerl::unordered_dense::set<IR::LocationDescriptor> InvalidateRanges(const boost::icl::interval_set<ProgramCounterType>& ranges);

private:
    boost::icl::interval_map<ProgramCounterType, std::set<IR::LocationDescriptor>> block_ranges;
};

}  // namespace Dynarmic::Backend
