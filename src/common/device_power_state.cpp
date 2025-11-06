// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "device_power_state.h"

#if defined(_WIN32)
#include <windows.h>

#elif defined(__ANDROID__)
#include <atomic>
extern std::atomic<int> g_battery_percentage;
extern std::atomic<bool> g_is_charging;
extern std::atomic<bool> g_has_battery;

#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_MAC
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#endif

#elif defined(__linux__)
#include <fstream>
#include <string>
#include <dirent.h>
#endif

namespace Common {

    PowerStatus GetPowerStatus()
    {
        PowerStatus info;

#if defined(_WIN32)
        SYSTEM_POWER_STATUS status;
        if (GetSystemPowerStatus(&status)) {
            if (status.BatteryFlag == 128) {
                info.has_battery = false;
            } else {
                info.percentage = status.BatteryLifePercent;
                info.charging = (status.BatteryFlag & 8) != 0;
            }
        } else {
            info.has_battery = false;
        }

#elif defined(__ANDROID__)
        info.percentage = g_battery_percentage.load(std::memory_order_relaxed);
        info.charging = g_is_charging.load(std::memory_order_relaxed);
        info.has_battery = g_has_battery.load(std::memory_order_relaxed);

#elif defined(__APPLE__) && TARGET_OS_MAC
        CFTypeRef info_ref = IOPSCopyPowerSourcesInfo();
        CFArrayRef sources = IOPSCopyPowerSourcesList(info_ref);
        if (CFArrayGetCount(sources) > 0) {
            CFDictionaryRef battery =
                IOPSGetPowerSourceDescription(info_ref, CFArrayGetValueAtIndex(sources, 0));

            CFNumberRef curNum =
                (CFNumberRef)CFDictionaryGetValue(battery, CFSTR(kIOPSCurrentCapacityKey));
            CFNumberRef maxNum =
                (CFNumberRef)CFDictionaryGetValue(battery, CFSTR(kIOPSMaxCapacityKey));
            int cur = 0, max = 0;
            CFNumberGetValue(curNum, kCFNumberIntType, &cur);
            CFNumberGetValue(maxNum, kCFNumberIntType, &max);

            if (max > 0)
                info.percentage = (cur * 100) / max;

            CFBooleanRef isCharging =
                (CFBooleanRef)CFDictionaryGetValue(battery, CFSTR(kIOPSIsChargingKey));
            info.charging = CFBooleanGetValue(isCharging);
        } else {
            info.has_battery = false;
        }
        CFRelease(sources);
        CFRelease(info_ref);

#elif defined(__linux__)
        const char* battery_path = "/sys/class/power_supply/BAT0/";

        std::ifstream capFile(std::string(battery_path) + "capacity");
        if (capFile) {
            capFile >> info.percentage;
        }
        else {
            info.has_battery = false;
        }

        std::ifstream statFile(std::string(battery_path) + "status");
        if (statFile) {
            std::string status;
            std::getline(statFile, status);
            info.charging = (status == "Charging");
        }
#else
        info.has_battery = false;
#endif

        return info;
    }
}
