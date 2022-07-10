// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "video_core/vulkan_common/vulkan_wrapper.h"

#ifdef _WIN32
#include <cstring> // for memset, strncpy
#include <processthreadsapi.h>
#include <windows.h>
#elif defined(YUZU_UNIX)
#include <errno.h>
#include <sys/wait.h>
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

void CheckVulkan() {
    try {
        Vulkan::vk::InstanceDispatch dld;
        const Common::DynamicLibrary library = Vulkan::OpenLibrary();
        const Vulkan::vk::Instance instance =
            Vulkan::CreateInstance(library, dld, VK_API_VERSION_1_0);

    } catch (const Vulkan::vk::Exception& exception) {
        LOG_ERROR(Frontend, "Failed to initialize Vulkan: {}", exception.what());
    }
}

bool StartupChecks(const char* arg0, bool* has_broken_vulkan) {
#ifdef _WIN32
    // Check environment variable to see if we are the child
    char variable_contents[32];
    const DWORD startup_check_var =
        GetEnvironmentVariable(STARTUP_CHECK_ENV_VAR, variable_contents, 32);
    const std::string variable_contents_s{variable_contents};
    if (startup_check_var > 0 && variable_contents_s == "ON") {
        CheckVulkan();
        return true;
    }

    // Set the startup variable for child processes
    const bool env_var_set = SetEnvironmentVariableA(STARTUP_CHECK_ENV_VAR, "ON");
    if (!env_var_set) {
        std::fprintf(stderr, "SetEnvironmentVariableA failed to set %s with error %d\n",
                     STARTUP_CHECK_ENV_VAR, GetLastError());
        return false;
    }

    PROCESS_INFORMATION process_info;
    std::memset(&process_info, '\0', sizeof(process_info));

    if (!SpawnChild(arg0, &process_info)) {
        return false;
    }

    // wait until the processs exits
    DWORD exit_code = STILL_ACTIVE;
    while (exit_code == STILL_ACTIVE) {
        const int err = GetExitCodeProcess(process_info.hProcess, &exit_code);
        if (err == 0) {
            std::fprintf(stderr, "GetExitCodeProcess failed with error %d\n", GetLastError());
            break;
        }
    }

    *has_broken_vulkan = (exit_code != 0);

    if (CloseHandle(process_info.hProcess) == 0) {
        std::fprintf(stderr, "CloseHandle failed with error %d\n", GetLastError());
    }
    if (CloseHandle(process_info.hThread) == 0) {
        std::fprintf(stderr, "CloseHandle failed with error %d\n", GetLastError());
    }

#elif defined(YUZU_UNIX)
    const pid_t pid = fork();
    if (pid == 0) {
        CheckVulkan();
        return true;
    } else if (pid == -1) {
        const int err = errno;
        std::fprintf(stderr, "fork failed with error %d\n", err);
        return false;
    }

    int status;
    const int r_val = wait(&status);
    if (r_val == -1) {
        const int err = errno;
        std::fprintf(stderr, "wait failed with error %d\n", err);
        return false;
    }
    *has_broken_vulkan = (status != 0);
#endif
    return false;
}

#ifdef _WIN32
bool SpawnChild(const char* arg0, PROCESS_INFORMATION* pi) {
    STARTUPINFOA startup_info;

    std::memset(&startup_info, '\0', sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);

    char p_name[255];
    std::strncpy(p_name, arg0, 255);

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
                                                pi             // lpProcessInformation
    );
    if (!process_created) {
        std::fprintf(stderr, "CreateProcessA failed with error %d\n", GetLastError());
        return false;
    }

    return true;
}
#endif
