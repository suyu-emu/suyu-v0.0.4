# SPDX-FileCopyrightText: 2017 Citra Emulator Project & 2024 suyu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_executable(suyu-room
    precompiled_headers.h
    suyu_room.cpp
    suyu_room.rc
)

target_link_libraries(suyu-room PRIVATE common network)
if (ENABLE_WEB_SERVICE)
    target_compile_definitions(suyu-room PRIVATE -DENABLE_WEB_SERVICE)
    target_link_libraries(suyu-room PRIVATE web_service)
endif()

target_link_libraries(suyu-room PRIVATE mbedtls mbedcrypto)
if (MSVC)
    target_link_libraries(suyu-room PRIVATE getopt)
endif()
target_link_libraries(suyu-room PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

if(UNIX AND NOT APPLE)
    install(TARGETS suyu-room)
endif()

if (SUYU_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(suyu-room PRIVATE precompiled_headers.h)
endif()

create_target_directory_groups(suyu-room)
