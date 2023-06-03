// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>
#include <exception>
#include <iomanip>
#include <sstream>
#include <stdexcept>
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

// Key is [Hours * 100 + Minutes], multiplied by 100 if DST
const static std::map<s64, const char*> off_timezones = {
    {530, "Asia/Calcutta"},          {930, "Australia/Darwin"},     {845, "Australia/Eucla"},
    {103000, "Australia/Adelaide"},  {1030, "Australia/Lord_Howe"}, {630, "Indian/Cocos"},
    {1245, "Pacific/Chatham"},       {134500, "Pacific/Chatham"},   {-330, "Canada/Newfoundland"},
    {-23000, "Canada/Newfoundland"}, {430, "Asia/Kabul"},           {330, "Asia/Tehran"},
    {43000, "Asia/Tehran"},          {545, "Asia/Kathmandu"},       {-930, "Asia/Marquesas"},
};

std::string FindSystemTimeZone() {
#if defined(MINGW)
    // MinGW has broken strftime -- https://sourceforge.net/p/mingw-w64/bugs/793/
    // e.g. fmt::format("{:%z}") -- returns "Eastern Daylight Time" when it should be "-0400"
    return timezones[0];
#else
    const s64 seconds = static_cast<s64>(GetCurrentOffsetSeconds().count());

    const s64 minutes = seconds / 60;
    const s64 hours = minutes / 60;

    const s64 minutes_off = minutes - hours * 60;

    if (minutes_off != 0) {
        const auto the_time = std::time(nullptr);
        const struct std::tm& local = *std::localtime(&the_time);
        const bool is_dst = local.tm_isdst != 0;

        const s64 tz_index = (hours * 100 + minutes_off) * (is_dst ? 100 : 1);

        try {
            return off_timezones.at(tz_index);
        } catch (std::out_of_range&) {
            LOG_ERROR(Common, "Time zone {} not handled, defaulting to hour offset.", tz_index);
        }
    }
    return fmt::format("Etc/GMT{:s}{:d}", hours > 0 ? "-" : "+", std::abs(hours));
#endif
}

} // namespace Common::TimeZone
