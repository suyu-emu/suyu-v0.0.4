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
    const bool env_var_set = SetEnvironmentVariableA(STARTUP_CHECK_ENV_VAR, "ON");
    if (!env_var_set) {
        std::fprintf(stderr, "SetEnvironmentVariableA failed to set %s, %d\n",
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
        GetExitCodeProcess(process_info.hProcess, &exit_code);
    }

    *has_broken_vulkan = (exit_code != 0);

    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
#endif
    return true;
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
        std::fprintf(stderr, "CreateProcessA failed, %d\n", GetLastError());
        return false;
    }

    return true;
}
#endif
