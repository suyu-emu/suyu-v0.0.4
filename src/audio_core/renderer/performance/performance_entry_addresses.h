// SPDX-FileCopyrightText: Copyright 2022 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "audio_core/common/common.h"

namespace AudioCore::Renderer {

struct PerformanceEntryAddresses {
    CpuAddr translated_address;
    CpuAddr entry_start_time_offset;
    CpuAddr header_entry_count_offset;
    CpuAddr entry_processed_time_offset;
};

} // namespace AudioCore::Renderer
