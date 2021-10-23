// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included

#pragma once

#include "common/common_types.h"
#include "common/swap.h"

namespace Service::HID {
constexpr std::size_t max_entry_size = 17;

template <typename State>
struct AtomicStorage {
    s64 sampling_number;
    State state;
};

template <typename State>
struct Lifo {
    s64 timestamp{};
    s64 total_entry_count = max_entry_size;
    s64 last_entry_index{};
    s64 entry_count{};
    std::array<AtomicStorage<State>, max_entry_size> entries{};

    const AtomicStorage<State>& ReadCurrentEntry() const {
        return entries[last_entry_index];
    }

    const AtomicStorage<State>& ReadPreviousEntry() const {
        return entries[GetPreviuousEntryIndex()];
    }

    std::size_t GetPreviuousEntryIndex() const {
        return (last_entry_index + total_entry_count - 1) % total_entry_count;
    }

    std::size_t GetNextEntryIndex() const {
        return (last_entry_index + 1) % total_entry_count;
    }

    void WriteNextEntry(const State& new_state) {
        if (entry_count < total_entry_count - 1) {
            entry_count++;
        }
        last_entry_index = GetNextEntryIndex();
        const auto& previous_entry = ReadPreviousEntry();
        entries[last_entry_index].sampling_number = previous_entry.sampling_number + 1;
        entries[last_entry_index].state = new_state;
    }
};

} // namespace Service::HID
