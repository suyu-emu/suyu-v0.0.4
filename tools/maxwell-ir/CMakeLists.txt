# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(maxwell-ir main.cpp)
target_link_libraries(maxwell-ir PRIVATE common shader_recompiler Threads::Threads)
target_include_directories(maxwell-ir PRIVATE ${CMAKE_SOURCE_DIR}/src)
if(UNIX AND NOT APPLE)
    install(TARGETS maxwell-ir)
endif()
create_target_directory_groups(maxwell-ir)
