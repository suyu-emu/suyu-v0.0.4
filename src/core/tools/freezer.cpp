// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/memory.h"
#include "core/tools/freezer.h"

namespace Tools {

namespace {

constexpr s64 MEMORY_FREEZER_TICKS = static_cast<s64>(Core::Timing::BASE_CLOCK_RATE / 60);

u64 MemoryReadWidth(u32 width, VAddr addr) {
    switch (width) {
    case 1:
        return Memory::Read8(addr);
    case 2:
        return Memory::Read16(addr);
    case 4:
        return Memory::Read32(addr);
    case 8:
        return Memory::Read64(addr);
    default:
        UNREACHABLE();
        return 0;
    }
}

void MemoryWriteWidth(u32 width, VAddr addr, u64 value) {
    switch (width) {
    case 1:
        Memory::Write8(addr, static_cast<u8>(value));
        break;
    case 2:
        Memory::Write16(addr, static_cast<u16>(value));
        break;
    case 4:
        Memory::Write32(addr, static_cast<u32>(value));
        break;
    case 8:
        Memory::Write64(addr, value);
        break;
    default:
        UNREACHABLE();
    }
}

} // Anonymous namespace

Freezer::Freezer(Core::Timing::CoreTiming& core_timing) : core_timing(core_timing) {
    event = Core::Timing::CreateEvent(
        "MemoryFreezer::FrameCallback",
        [this](u64 userdata, s64 cycles_late) { FrameCallback(userdata, cycles_late); });
    core_timing.ScheduleEvent(MEMORY_FREEZER_TICKS, event);
}

Freezer::~Freezer() {
    core_timing.UnscheduleEvent(event, 0);
}

void Freezer::SetActive(bool active) {
    if (!this->active.exchange(active)) {
        FillEntryReads();
        core_timing.ScheduleEvent(MEMORY_FREEZER_TICKS, event);
        LOG_DEBUG(Common_Memory, "Memory freezer activated!");
    } else {
        LOG_DEBUG(Common_Memory, "Memory freezer deactivated!");
    }
}

bool Freezer::IsActive() const {
    return active.load(std::memory_order_relaxed);
}

void Freezer::Clear() {
    std::lock_guard lock{entries_mutex};

    LOG_DEBUG(Common_Memory, "Clearing all frozen memory values.");

    entries.clear();
}

u64 Freezer::Freeze(VAddr address, u32 width) {
    std::lock_guard lock{entries_mutex};

    const auto current_value = MemoryReadWidth(width, address);
    entries.push_back({address, width, current_value});

    LOG_DEBUG(Common_Memory,
              "Freezing memory for address={:016X}, width={:02X}, current_value={:016X}", address,
              width, current_value);

    return current_value;
}

void Freezer::Unfreeze(VAddr address) {
    std::lock_guard lock{entries_mutex};

    LOG_DEBUG(Common_Memory, "Unfreezing memory for address={:016X}", address);

    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
                       [&address](const Entry& entry) { return entry.address == address; }),
        entries.end());
}

bool Freezer::IsFrozen(VAddr address) const {
    std::lock_guard lock{entries_mutex};

    return std::find_if(entries.begin(), entries.end(), [&address](const Entry& entry) {
               return entry.address == address;
           }) != entries.end();
}

void Freezer::SetFrozenValue(VAddr address, u64 value) {
    std::lock_guard lock{entries_mutex};

    const auto iter = std::find_if(entries.begin(), entries.end(), [&address](const Entry& entry) {
        return entry.address == address;
    });

    if (iter == entries.end()) {
        LOG_ERROR(Common_Memory,
                  "Tried to set freeze value for address={:016X} that is not frozen!", address);
        return;
    }

    LOG_DEBUG(Common_Memory,
              "Manually overridden freeze value for address={:016X}, width={:02X} to value={:016X}",
              iter->address, iter->width, value);
    iter->value = value;
}

std::optional<Freezer::Entry> Freezer::GetEntry(VAddr address) const {
    std::lock_guard lock{entries_mutex};

    const auto iter = std::find_if(entries.begin(), entries.end(), [&address](const Entry& entry) {
        return entry.address == address;
    });

    if (iter == entries.end()) {
        return std::nullopt;
    }

    return *iter;
}

std::vector<Freezer::Entry> Freezer::GetEntries() const {
    std::lock_guard lock{entries_mutex};

    return entries;
}

void Freezer::FrameCallback(u64 userdata, s64 cycles_late) {
    if (!IsActive()) {
        LOG_DEBUG(Common_Memory, "Memory freezer has been deactivated, ending callback events.");
        return;
    }

    std::lock_guard lock{entries_mutex};

    for (const auto& entry : entries) {
        LOG_DEBUG(Common_Memory,
                  "Enforcing memory freeze at address={:016X}, value={:016X}, width={:02X}",
                  entry.address, entry.value, entry.width);
        MemoryWriteWidth(entry.width, entry.address, entry.value);
    }

    core_timing.ScheduleEvent(MEMORY_FREEZER_TICKS - cycles_late, event);
}

void Freezer::FillEntryReads() {
    std::lock_guard lock{entries_mutex};

    LOG_DEBUG(Common_Memory, "Updating memory freeze entries to current values.");

    for (auto& entry : entries) {
        entry.value = MemoryReadWidth(entry.width, entry.address);
    }
}

} // namespace Tools
