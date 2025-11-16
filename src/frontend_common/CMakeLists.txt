# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(frontend_common STATIC
    config.cpp
    config.h
    content_manager.h
    firmware_manager.h
    firmware_manager.cpp
    data_manager.h data_manager.cpp
    play_time_manager.cpp
    play_time_manager.h
)

if (ENABLE_UPDATE_CHECKER)
    target_link_libraries(frontend_common PRIVATE httplib::httplib)
    target_link_libraries(frontend_common PRIVATE nlohmann_json::nlohmann_json)
    target_sources(frontend_common PRIVATE
        update_checker.cpp
        update_checker.h
    )

    if (ENABLE_OPENSSL)
        target_compile_definitions(frontend_common PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
        target_link_libraries(frontend_common PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()

endif()

create_target_directory_groups(frontend_common)
target_link_libraries(frontend_common PUBLIC core SimpleIni::SimpleIni PRIVATE common Boost::headers)
