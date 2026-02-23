# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2017 Citra Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(yuzu-room STATIC EXCLUDE_FROM_ALL
    yuzu_room.cpp
    yuzu_room.h
    yuzu_room.rc
)

target_link_libraries(yuzu-room PRIVATE common network)
if (ENABLE_WEB_SERVICE)
    target_compile_definitions(yuzu-room PRIVATE ENABLE_WEB_SERVICE)
    target_link_libraries(yuzu-room PRIVATE web_service)
endif()

target_link_libraries(yuzu-room PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto)

if (MSVC)
    target_link_libraries(yuzu-room PRIVATE getopt)
endif()
target_link_libraries(yuzu-room PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

create_target_directory_groups(yuzu-room)
