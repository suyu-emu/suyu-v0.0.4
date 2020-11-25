// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <mutex>
#include <thread>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include "common/uint128.h"
#include "common/x64/native_clock.h"

namespace Common {

u64 EstimateRDTSCFrequency() {
    const auto milli_10 = std::chrono::milliseconds{10};
    // get current time
    _mm_mfence();
    const u64 tscStart = __rdtsc();
    const auto startTime = std::chrono::high_resolution_clock::now();
    // wait roughly 3 seconds
    while (true) {
        auto milli = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - startTime);
        if (milli.count() >= 3000)
            break;
        std::this_thread::sleep_for(milli_10);
    }
    const auto endTime = std::chrono::high_resolution_clock::now();
    _mm_mfence();
    const u64 tscEnd = __rdtsc();
    // calculate difference
    const u64 timer_diff =
        std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();
    const u64 tsc_diff = tscEnd - tscStart;
    const u64 tsc_freq = MultiplyAndDivide64(tsc_diff, 1000000000ULL, timer_diff);
    return tsc_freq;
}

namespace X64 {
NativeClock::NativeClock(u64 emulated_cpu_frequency_, u64 emulated_clock_frequency_,
                         u64 rtsc_frequency_)
    : WallClock(emulated_cpu_frequency_, emulated_clock_frequency_, true), rtsc_frequency{
                                                                               rtsc_frequency_} {
    _mm_mfence();
    last_measure = __rdtsc();
    accumulated_ticks = 0U;
}

u64 NativeClock::GetRTSC() {
    std::scoped_lock scope{rtsc_serialize};
    _mm_mfence();
    const u64 current_measure = __rdtsc();
    u64 diff = current_measure - last_measure;
    diff = diff & ~static_cast<u64>(static_cast<s64>(diff) >> 63); // max(diff, 0)
    if (current_measure > last_measure) {
        last_measure = current_measure;
    }
    accumulated_ticks += diff;
    /// The clock cannot be more precise than the guest timer, remove the lower bits
    return accumulated_ticks & inaccuracy_mask;
}

void NativeClock::Pause(bool is_paused) {
    if (!is_paused) {
        _mm_mfence();
        last_measure = __rdtsc();
    }
}

std::chrono::nanoseconds NativeClock::GetTimeNS() {
    const u64 rtsc_value = GetRTSC();
    return std::chrono::nanoseconds{MultiplyAndDivide64(rtsc_value, 1000000000, rtsc_frequency)};
}

std::chrono::microseconds NativeClock::GetTimeUS() {
    const u64 rtsc_value = GetRTSC();
    return std::chrono::microseconds{MultiplyAndDivide64(rtsc_value, 1000000, rtsc_frequency)};
}

std::chrono::milliseconds NativeClock::GetTimeMS() {
    const u64 rtsc_value = GetRTSC();
    return std::chrono::milliseconds{MultiplyAndDivide64(rtsc_value, 1000, rtsc_frequency)};
}

u64 NativeClock::GetClockCycles() {
    const u64 rtsc_value = GetRTSC();
    return MultiplyAndDivide64(rtsc_value, emulated_clock_frequency, rtsc_frequency);
}

u64 NativeClock::GetCPUCycles() {
    const u64 rtsc_value = GetRTSC();
    return MultiplyAndDivide64(rtsc_value, emulated_cpu_frequency, rtsc_frequency);
}

} // namespace X64

} // namespace Common
