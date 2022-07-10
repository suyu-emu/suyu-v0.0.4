// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "video_core/vulkan_common/vulkan_wrapper.h"

#ifdef _WIN32
#include <cstring> // for memset, strncpy
#include <processthreadsapi.h>
#include <windows.h>
#elif defined(YUZU_UNIX)
#include <unistd.h>
#endif

#include <cstdio>
#include <filesystem>
#include <fstream>
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "video_core/vulkan_common/vulkan_instance.h"
#include "video_core/vulkan_common/vulkan_library.h"
#include "yuzu/startup_checks.h"
#include "yuzu/uisettings.h"

constexpr char TEMP_FILE_NAME[] = "vulkan_check";

bool CheckVulkan() {
    if (UISettings::values.has_broken_vulkan) {
        return true;
    }

    LOG_DEBUG(Frontend, "Checking presence of Vulkan");

    const auto fs_config_loc = Common::FS::GetYuzuPath(Common::FS::YuzuPath::ConfigDir);
    const auto temp_file_loc = fs_config_loc / TEMP_FILE_NAME;

    if (std::filesystem::exists(temp_file_loc)) {
        LOG_WARNING(Frontend, "Detected recovery from previous failed Vulkan initialization");

        UISettings::values.has_broken_vulkan = true;
        std::filesystem::remove(temp_file_loc);
        return false;
    }

    std::ofstream temp_file_handle(temp_file_loc);
    temp_file_handle.close();

    try {
        Vulkan::vk::InstanceDispatch dld;
        const Common::DynamicLibrary library = Vulkan::OpenLibrary();
        const Vulkan::vk::Instance instance =
            Vulkan::CreateInstance(library, dld, VK_API_VERSION_1_0);

    } catch (const Vulkan::vk::Exception& exception) {
        LOG_ERROR(Frontend, "Failed to initialize Vulkan: {}", exception.what());
        // Don't set has_broken_vulkan to true here: we care when loading Vulkan crashes the
        // application, not when we can handle it.
    }

    std::filesystem::remove(temp_file_loc);
    return true;
}

bool StartupChecks() {
#ifdef _WIN32
    const bool env_var_set = SetEnvironmentVariableA(STARTUP_CHECK_ENV_VAR, "ON");
    if (!env_var_set) {
        LOG_ERROR(Frontend, "SetEnvironmentVariableA failed to set {}, {}", STARTUP_CHECK_ENV_VAR,
                  GetLastError());
        return false;
    }

    STARTUPINFOA startup_info;
    PROCESS_INFORMATION process_info;

    std::memset(&startup_info, '\0', sizeof(startup_info));
    std::memset(&process_info, '\0', sizeof(process_info));
    startup_info.cb = sizeof(startup_info);

    char p_name[255];
    std::strncpy(p_name, "yuzu.exe", 255);

    // TODO: use argv[0] instead of yuzu.exe
    const bool process_created = CreateProcessA(nullptr,       // lpApplicationName
                                                p_name,        // lpCommandLine
                                                nullptr,       // lpProcessAttributes
                                                nullptr,       // lpThreadAttributes
                                                false,         // bInheritHandles
                                                0,             // dwCreationFlags
                                                nullptr,       // lpEnvironment
                                                nullptr,       // lpCurrentDirectory
                                                &startup_info, // lpStartupInfo
                                                &process_info  // lpProcessInformation
    );
    if (!process_created) {
        LOG_ERROR(Frontend, "CreateProcessA failed, {}", GetLastError());
        return false;
    }

    // wait until the processs exits
    DWORD exit_code = STILL_ACTIVE;
    while (exit_code == STILL_ACTIVE) {
        GetExitCodeProcess(process_info.hProcess, &exit_code);
    }

    std::fprintf(stderr, "exit code: %d\n", exit_code);

    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
#endif
    return true;
}
