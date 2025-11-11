# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(yuzu_room_standalone
    yuzu_room_standalone.cpp
)

set_target_properties(yuzu_room_standalone PROPERTIES OUTPUT_NAME "eden-room")

target_link_libraries(yuzu_room_standalone PRIVATE yuzu-room)

if(UNIX AND NOT APPLE)
    install(TARGETS yuzu_room_standalone RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

create_target_directory_groups(yuzu_room_standalone)
