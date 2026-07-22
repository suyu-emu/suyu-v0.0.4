# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(maxwell-spirv
    main.cpp
    spirv_recompiler_impl.cpp
    spirv_reference_impl.cpp
)
target_link_libraries(maxwell-spirv PRIVATE common shader_recompiler Threads::Threads)
target_include_directories(maxwell-spirv PRIVATE ${CMAKE_SOURCE_DIR}/src)
if(UNIX AND NOT APPLE)
    install(TARGETS maxwell-spirv)
endif()
create_target_directory_groups(maxwell-spirv)
