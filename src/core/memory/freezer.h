// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <vector>
#include "common/common_types.h"
#include "core/core_timing.h"

namespace Core {
class System;
} // namespace Core

namespace Memory {

// A class that will effectively freeze memory values.
class Freezer {
public:
    struct Entry {
        VAddr address;
        u8 width;
        u64 value;
    };

    Freezer(Core::Timing::CoreTiming& core_timing);
    ~Freezer();

    void SetActive(bool active);
    bool IsActive() const;

    void Clear();

    u64 Freeze(VAddr address, u8 width);
    void Unfreeze(VAddr address);

    bool IsFrozen(VAddr address);
    void SetFrozenValue(VAddr address, u64 value);

    std::optional<Entry> GetEntry(VAddr address);

    std::vector<Entry> GetEntries();

private:
    void FrameCallback(u64 userdata, s64 cycles_late);
    void FillEntryReads();

    std::atomic_bool active{false};

    std::recursive_mutex entries_mutex;
    std::vector<Entry> entries;

    Core::Timing::EventType* event;
    Core::Timing::CoreTiming& core_timing;
};

} // namespace Memory
