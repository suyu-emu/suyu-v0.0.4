// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>
#include <iomanip>
#include <sstream>
#include <fmt/chrono.h>
#include <fmt/core.h>

#include "common/logging/log.h"
#include "common/settings.h"
#include "common/time_zone.h"

namespace Common::TimeZone {

// Time zone strings
constexpr std::array timezones{
    "GMT",       "GMT",       "CET", "CST6CDT", "Cuba",    "EET",    "Egypt",     "Eire",
    "EST",       "EST5EDT",   "GB",  "GB-Eire", "GMT",     "GMT+0",  "GMT-0",     "GMT0",
    "Greenwich", "Hongkong",  "HST", "Iceland", "Iran",    "Israel", "Jamaica",   "Japan",
    "Kwajalein", "Libya",     "MET", "MST",     "MST7MDT", "Navajo", "NZ",        "NZ-CHAT",
    "Poland",    "Portugal",  "PRC", "PST8PDT", "ROC",     "ROK",    "Singapore", "Turkey",
    "UCT",       "Universal", "UTC", "W-SU",    "WET",     "Zulu",
};

const std::array<const char*, 46>& GetTimeZoneStrings() {
    return timezones;
}

std::string GetDefaultTimeZone() {
    return "GMT";
}

static std::string GetOsTimeZoneOffset() {
    const std::time_t t{std::time(nullptr)};
    const std::tm tm{*std::localtime(&t)};

    return fmt::format("{:%z}", tm);
}

static int ConvertOsTimeZoneOffsetToInt(const std::string& timezone) {
    try {
        return std::stoi(timezone);
    } catch (const std::invalid_argument&) {
        LOG_CRITICAL(Common, "invalid_argument with {}!", timezone);
        return 0;
    } catch (const std::out_of_range&) {
        LOG_CRITICAL(Common, "out_of_range with {}!", timezone);
        return 0;
    }
}

std::chrono::seconds GetCurrentOffsetSeconds() {
    const int offset{ConvertOsTimeZoneOffsetToInt(GetOsTimeZoneOffset())};

    int seconds{(offset / 100) * 60 * 60}; // Convert hour component to seconds
    seconds += (offset % 100) * 60;        // Convert minute component to seconds

    return std::chrono::seconds{seconds};
}

std::string FindSystemTimeZone() {
#if defined(MINGW)
    // MinGW has broken strftime -- https://sourceforge.net/p/mingw-w64/bugs/793/
    // e.g. fmt::format("{:%z}") -- returns "Eastern Daylight Time" when it should be "-0400"
    return timezones[0];
#else
    // Time zone offset in seconds from GMT
    constexpr std::array offsets{
        0,     0,     3600,  -21600, -19768, 7200,   7509,   -1521, -18000, -18000, -75,    -75,
        0,     0,     0,     0,      0,      27402,  -36000, -968,  12344,  8454,   -18430, 33539,
        40160, 3164,  3600,  -25200, -25200, -25196, 41944,  44028, 5040,   -2205,  29143,  -28800,
        29160, 30472, 24925, 6952,   0,      0,      0,      9017,  0,      0,
    };

    // If the time zone recognizes Daylight Savings Time
    constexpr std::array dst{
        false, false, true,  true,  true,  true,  true,  true,  false, true,  true, true,
        false, false, false, false, false, true,  false, false, true,  true,  true, true,
        false, true,  true,  false, true,  true,  true,  true,  true,  true,  true, true,
        true,  true,  true,  true,  false, false, false, true,  true,  false,
    };

    static std::string system_time_zone_cached{};
    if (!system_time_zone_cached.empty()) {
        return system_time_zone_cached;
    }

    const auto now = std::time(nullptr);
    const struct std::tm& local = *std::localtime(&now);

    const s64 system_offset = GetCurrentOffsetSeconds().count() - (local.tm_isdst ? 3600 : 0);

    int min = std::numeric_limits<int>::max();
    int min_index = -1;
    for (u32 i = 2; i < offsets.size(); i++) {
        // Skip if system is celebrating DST but considered time zone does not
        if (local.tm_isdst && !dst[i]) {
            continue;
        }

        const auto offset = offsets[i];
        const int difference = static_cast<int>(std::abs(offset - system_offset));
        if (difference < min) {
            min = difference;
            min_index = i;
        }
    }

    system_time_zone_cached = GetTimeZoneStrings()[min_index];
    return system_time_zone_cached;
#endif
}

} // namespace Common::TimeZone
