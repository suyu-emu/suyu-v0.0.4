// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// You must ensure this matches with src/common/x64/xbyak.h on root dir
#include <ankerl/unordered_dense.h>
#include <boost/unordered_map.hpp>
#define XBYAK_STD_UNORDERED_SET ankerl::unordered_dense::set
#define XBYAK_STD_UNORDERED_MAP ankerl::unordered_dense::map
#define XBYAK_STD_UNORDERED_MULTIMAP boost::unordered_multimap
#include <xbyak/xbyak.h>
#include <xbyak/xbyak_util.h>
