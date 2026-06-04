// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2013 Dolphin Emulator Project / 2015 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <optional>
#include <algorithm>
#include <string_view>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#elif defined(__DragonFly__) || defined(__FreeBSD__)
#include <sys/types.h>
#include <machine/cpufunc.h>
#elif defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

#include "common/steady_clock.h"
#include "common/uint128.h"
#include "common/bit_util.h"
#include "common/common_types.h"
#include "common/cpu_features.h"
#include "common/logging.h"

#ifdef ARCHITECTURE_x86_64
#include "common/x64/rdtsc.h"
#ifdef _MSC_VER
#include <intrin.h>
static inline u64 xgetbv(u32 index) { return _xgetbv(index); }
#else
static inline void __cpuidex(int info[4], u32 function_id, u32 subfunction_id) {
#if defined(__DragonFly__) || defined(__FreeBSD__)
    // Despite the name, this is just do_cpuid() with ECX as second input.
    cpuid_count((u_int)function_id, (u_int)subfunction_id, (u_int*)info);
#else
    info[0] = function_id;    // eax
    info[2] = subfunction_id; // ecx
    __asm__("cpuid"
            : "=a"(info[0]), "=b"(info[1]), "=c"(info[2]), "=d"(info[3])
            : "a"(function_id), "c"(subfunction_id));
#endif
}
static inline void __cpuid(int info[4], u32 function_id) { return __cpuidex(info, function_id, 0); }
#define _XCR_XFEATURE_ENABLED_MASK 0
static inline u64 xgetbv(u32 index) {
    u32 eax, edx;
    __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
    return ((u64)edx << 32) | eax;
}
#endif // _MSC_VER
#endif

namespace Common {
#ifdef ARCHITECTURE_x86_64
CPUCaps::Manufacturer CPUCaps::ParseManufacturer(std::string_view brand_string) {
    if (brand_string == "GenuineIntel") {
        return Manufacturer::Intel;
    } else if (brand_string == "AuthenticAMD") {
        return Manufacturer::AMD;
    } else if (brand_string == "HygonGenuine") {
        return Manufacturer::Hygon;
    }
    return Manufacturer::Unknown;
}

std::optional<int> GetProcessorCount() {
#if defined(_WIN32)
    // Get the buffer length.
    DWORD length = 0;
    GetLogicalProcessorInformation(nullptr, &length);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        LOG_ERROR(Frontend, "Failed to query core count.");
        return std::nullopt;
    }
    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(
        length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
    // Now query the core count.
    if (!GetLogicalProcessorInformation(buffer.data(), &length)) {
        LOG_ERROR(Frontend, "Failed to query core count.");
        return std::nullopt;
    }
    return static_cast<int>(
        std::count_if(buffer.cbegin(), buffer.cend(), [](const auto& proc_info) {
            return proc_info.Relationship == RelationProcessorCore;
        }));
#elif defined(__unix__)
    const int thread_count = std::thread::hardware_concurrency();
    std::ifstream smt("/sys/devices/system/cpu/smt/active");
    char state = '0';
    if (smt) {
        smt.read(&state, sizeof(state));
    }
    switch (state) {
    case '0':
        return thread_count;
    case '1':
        return thread_count / 2;
    default:
        return std::nullopt;
    }
#else
    // Shame on you
    return std::nullopt;
#endif
}

/// @brief Detects the various CPU features
const CPUCaps g_cpu_caps = [] {
    CPUCaps caps = {};

    // Assumes the CPU supports the CPUID instruction. Those that don't would likely not support
    // yuzu at all anyway
    int cpu_id[4];

    // Detect CPU's CPUID capabilities and grab manufacturer string
    __cpuid(cpu_id, 0x00000000);
    const u32 max_std_fn = cpu_id[0]; // EAX

    std::memset(caps.brand_string, 0, std::size(caps.brand_string));
    std::memcpy(&caps.brand_string[0], &cpu_id[1], sizeof(u32));
    std::memcpy(&caps.brand_string[4], &cpu_id[3], sizeof(u32));
    std::memcpy(&caps.brand_string[8], &cpu_id[2], sizeof(u32));

    caps.manufacturer = CPUCaps::ParseManufacturer(caps.brand_string);

    // Set reasonable default cpu string even if brand string not available
    std::strncpy(caps.cpu_string, caps.brand_string, std::size(caps.brand_string));

    __cpuid(cpu_id, 0x80000000);

    const u32 max_ex_fn = cpu_id[0];

    // Detect family and other miscellaneous features
    if (max_std_fn >= 1) {
        __cpuid(cpu_id, 0x00000001);
        caps.sse3 = Common::Bit<0>(cpu_id[2]);
        caps.pclmulqdq = Common::Bit<1>(cpu_id[2]);
        caps.ssse3 = Common::Bit<9>(cpu_id[2]);
        caps.sse4_1 = Common::Bit<19>(cpu_id[2]);
        caps.sse4_2 = Common::Bit<20>(cpu_id[2]);
        caps.movbe = Common::Bit<22>(cpu_id[2]);
        caps.popcnt = Common::Bit<23>(cpu_id[2]);
        caps.aes = Common::Bit<25>(cpu_id[2]);
        caps.f16c = Common::Bit<29>(cpu_id[2]);

        // AVX support requires 3 separate checks:
        //  - Is the AVX bit set in CPUID?
        //  - Is the XSAVE bit set in CPUID?
        //  - XGETBV result has the XCR bit set.
        if (Common::Bit<28>(cpu_id[2]) && Common::Bit<27>(cpu_id[2])) {
            if ((xgetbv(_XCR_XFEATURE_ENABLED_MASK) & 0x6) == 0x6) {
                caps.avx = true;
                if (Common::Bit<12>(cpu_id[2]))
                    caps.fma = true;
            }
        }

        if (max_std_fn >= 7) {
            __cpuidex(cpu_id, 0x00000007, 0x00000000);
            // Can't enable AVX{2,512} unless the XSAVE/XGETBV checks above passed
            if (caps.avx) {
                caps.avx2 = Common::Bit<5>(cpu_id[1]);
                caps.avx512f = Common::Bit<16>(cpu_id[1]);
                caps.avx512dq = Common::Bit<17>(cpu_id[1]);
                caps.avx512cd = Common::Bit<28>(cpu_id[1]);
                caps.avx512bw = Common::Bit<30>(cpu_id[1]);
                caps.avx512vl = Common::Bit<31>(cpu_id[1]);
                caps.avx512vbmi = Common::Bit<1>(cpu_id[2]);
                caps.avx512bitalg = Common::Bit<12>(cpu_id[2]);
            }

            caps.bmi1 = Common::Bit<3>(cpu_id[1]);
            caps.bmi2 = Common::Bit<8>(cpu_id[1]);
            caps.sha = Common::Bit<29>(cpu_id[1]);

            caps.waitpkg = Common::Bit<5>(cpu_id[2]);
            caps.gfni = Common::Bit<8>(cpu_id[2]);
        }
    }

    if (max_ex_fn >= 0x80000004) {
        // Extract CPU model string
        __cpuid(cpu_id, 0x80000002);
        std::memcpy(caps.cpu_string, cpu_id, sizeof(cpu_id));
        __cpuid(cpu_id, 0x80000003);
        std::memcpy(caps.cpu_string + 16, cpu_id, sizeof(cpu_id));
        __cpuid(cpu_id, 0x80000004);
        std::memcpy(caps.cpu_string + 32, cpu_id, sizeof(cpu_id));
    }

    if (max_ex_fn >= 0x80000001) {
        // Check for more features
        __cpuid(cpu_id, 0x80000001);
        caps.lzcnt = Common::Bit<5>(cpu_id[2]);
        caps.monitorx = Common::Bit<29>(cpu_id[2]);
    }

    if (max_ex_fn >= 0x80000007) {
        __cpuid(cpu_id, 0x80000007);
        caps.invariant_tsc = Common::Bit<8>(cpu_id[3]);
    }

    if (max_std_fn >= 0x15) {
        __cpuid(cpu_id, 0x15);
        caps.tsc_crystal_ratio_denominator = cpu_id[0];
        caps.tsc_crystal_ratio_numerator = cpu_id[1];
        caps.crystal_frequency = cpu_id[2];
        // Some CPU models might not return a crystal frequency.
        // The CPU model can be detected to use the values from turbostat
        // https://github.com/torvalds/linux/blob/master/tools/power/x86/turbostat/turbostat.c#L5569
        // but it's easier to just estimate the TSC tick rate for these cases.
        if (caps.tsc_crystal_ratio_denominator) {
            caps.tsc_frequency = u64(caps.crystal_frequency)
                * caps.tsc_crystal_ratio_numerator / caps.tsc_crystal_ratio_denominator;
        } else {
            caps.tsc_frequency = X64::EstimateRDTSCFrequency();
        }
    }

    if (max_std_fn >= 0x16) {
        __cpuid(cpu_id, 0x16);
        caps.base_frequency = cpu_id[0];
        caps.max_frequency = cpu_id[1];
        caps.bus_frequency = cpu_id[2];
    }
    return caps;
}();

#else

#endif

#if defined(ARCHITECTURE_x86_64)
WallClock::WallClock(bool invariant_, u64 rdtsc_frequency_) noexcept
    : rdtsc_frequency{rdtsc_frequency_}
    , ns_rdtsc_factor{invariant_ ? GetFixedPoint64Factor(NsRatio::den, rdtsc_frequency_) : 0}
    , us_rdtsc_factor{invariant_ ? GetFixedPoint64Factor(UsRatio::den, rdtsc_frequency_) : 0}
    , ms_rdtsc_factor{invariant_ ? GetFixedPoint64Factor(MsRatio::den, rdtsc_frequency_) : 0}
    , rdtsc_ns_factor{invariant_ ? GetFixedPoint64Factor(rdtsc_frequency_, NsRatio::den) : 1}
    , cntpct_rdtsc_factor{invariant_ ? GetFixedPoint64Factor(CNTFRQ, rdtsc_frequency_) : 0}
    , gputick_rdtsc_factor{invariant_ ? GetFixedPoint64Factor(GPUTickFreq, rdtsc_frequency_) : 0}
    , invariant{invariant_}
{}

std::chrono::nanoseconds WallClock::GetTimeNS() const {
    if (!invariant)
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch());
    return std::chrono::nanoseconds{MultiplyHigh(GetUptime(), ns_rdtsc_factor)};
}

std::chrono::microseconds WallClock::GetTimeUS() const {
    if (!invariant)
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
    return std::chrono::microseconds{MultiplyHigh(GetUptime(), us_rdtsc_factor)};
}

std::chrono::milliseconds WallClock::GetTimeMS() const {
    if (!invariant)
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
    return std::chrono::milliseconds{MultiplyHigh(GetUptime(), ms_rdtsc_factor)};
}

s64 WallClock::GetCNTPCT() const {
    if (!invariant)
        return GetUptime() * NsToCNTPCTRatio::num / NsToCNTPCTRatio::den;
    return MultiplyHigh(GetUptime(), cntpct_rdtsc_factor);
}

s64 WallClock::GetGPUTick() const {
    if (!invariant)
        return GetUptime() * NsToGPUTickRatio::num / NsToGPUTickRatio::den;
    return MultiplyHigh(GetUptime(), gputick_rdtsc_factor);
}

s64 WallClock::GetUptime() const {
    if (!invariant)
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return s64(Common::X64::FencedRDTSC());
}

bool WallClock::IsNative() const {
    return invariant;
}

u64 WallClock::NsToTicks(std::chrono::nanoseconds ns) const {
    return invariant ? MultiplyHigh(ns.count(), rdtsc_ns_factor) : ns.count();
}
#elif defined(HAS_NCE)
namespace {
[[nodiscard]] Common::WallClock::FactorType GetFixedPointFactor(u64 num, u64 den) noexcept {
    return (Common::WallClock::FactorType(num) << 64) / den;
}
[[nodiscard]] u64 MultiplyHigh(u64 m, Common::WallClock::FactorType factor) noexcept {
    return static_cast<u64>((m * factor) >> 64);
}
[[nodiscard]] s64 GetHostCNTFRQ() noexcept {
    u64 cntfrq_el0 = 0;
#ifdef __ANDROID__
    std::string_view board{""};
    char buffer[PROP_VALUE_MAX];
    int len{__system_property_get("ro.product.board", buffer)};
    board = std::string_view(buffer, static_cast<size_t>(len));
    if (board == "s5e9925") { // Exynos 2200
        cntfrq_el0 = 25600000;
    } else if (board == "exynos2100") { // Exynos 2100
        cntfrq_el0 = 26000000;
    } else if (board == "exynos9810") { // Exynos 9810
        cntfrq_el0 = 26000000;
    } else if (board == "s5e8825") { // Exynos 1280
        cntfrq_el0 = 26000000;
    } else {
        asm volatile("mrs %[cntfrq_el0], cntfrq_el0" : [cntfrq_el0] "=r"(cntfrq_el0));
    }
    return cntfrq_el0;
#else
    asm volatile("mrs %[cntfrq_el0], cntfrq_el0" : [cntfrq_el0] "=r"(cntfrq_el0));
    return cntfrq_el0;
#endif
}
} // namespace

WallClock::WallClock(bool invariant_, u64 rdtsc_frequency_) noexcept {
    const u64 host_cntfrq = std::max<u64>(GetHostCNTFRQ(), 1);
    ns_cntfrq_factor = GetFixedPointFactor(NsRatio::den, host_cntfrq);
    us_cntfrq_factor = GetFixedPointFactor(UsRatio::den, host_cntfrq);
    ms_cntfrq_factor = GetFixedPointFactor(MsRatio::den, host_cntfrq);
    cntfrq_ns_factor = GetFixedPointFactor(host_cntfrq, NsRatio::den);
    guest_cntfrq_factor = GetFixedPointFactor(CNTFRQ, host_cntfrq);
    gputick_cntfrq_factor = GetFixedPointFactor(GPUTickFreq, host_cntfrq);
}

std::chrono::nanoseconds WallClock::GetTimeNS() const {
    return std::chrono::nanoseconds{MultiplyHigh(GetUptime(), ns_cntfrq_factor)};
}

std::chrono::microseconds WallClock::GetTimeUS() const {
    return std::chrono::microseconds{MultiplyHigh(GetUptime(), us_cntfrq_factor)};
}

std::chrono::milliseconds WallClock::GetTimeMS() const {
    return std::chrono::milliseconds{MultiplyHigh(GetUptime(), ms_cntfrq_factor)};
}

s64 WallClock::GetCNTPCT() const {
    return MultiplyHigh(GetUptime(), guest_cntfrq_factor);
}

s64 WallClock::GetGPUTick() const {
    return MultiplyHigh(GetUptime(), gputick_cntfrq_factor);
}

s64 WallClock::GetUptime() const {
    s64 cntvct_el0 = 0;
    asm volatile(
        "dsb ish\n\t"
        "mrs %[cntvct_el0], cntvct_el0\n\t"
        "dsb ish\n\t"
        : [cntvct_el0] "=r"(cntvct_el0)
    );
    return cntvct_el0;
}

bool WallClock::IsNative() const {
    return true;
}

u64 WallClock::NsToTicks(std::chrono::nanoseconds ns) const {
    return MultiplyHigh(ns.count(), cntfrq_ns_factor);
}
#else
WallClock::WallClock(bool invariant_, u64 rdtsc_frequency_) noexcept {}

std::chrono::nanoseconds WallClock::GetTimeNS() const {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch());
}

std::chrono::microseconds WallClock::GetTimeUS() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
}

std::chrono::milliseconds WallClock::GetTimeMS() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
}

s64 WallClock::GetCNTPCT() const {
    return GetUptime() * NsToCNTPCTRatio::num / NsToCNTPCTRatio::den;
}

s64 WallClock::GetGPUTick() const {
    return GetUptime() * NsToGPUTickRatio::num / NsToGPUTickRatio::den;
}

s64 WallClock::GetUptime() const {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool WallClock::IsNative() const {
    return false;
}

u64 WallClock::NsToTicks(std::chrono::nanoseconds ns) const {
    return ns.count();
}
#endif

// Wall clock MUST be initialized AFTER g_cpu_caps
// C++ only guarantees ctor init in the order they appear in TU
const WallClock g_wall_clock = [] {
#if defined(ARCHITECTURE_x86_64)
    auto const& caps = Common::g_cpu_caps;
    return WallClock(caps.invariant_tsc && caps.tsc_frequency >= std::nano::den, caps.tsc_frequency);
#elif defined(HAS_NCE)
    return WallClock(false, 1);
#else
    return WallClock(true, 1);
#endif
}();
} // namespace Common
