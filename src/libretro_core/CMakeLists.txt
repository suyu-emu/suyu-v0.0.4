# SPDX-FileCopyrightText: Copyright 2026 suyu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(suyu_libretro SHARED
    libretro.h
    retro_core.cpp
    retro_emu_window.cpp
    retro_emu_window.h
)

target_link_libraries(suyu_libretro PRIVATE common core input_common frontend_common audio_core)
target_link_libraries(suyu_libretro PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

set_target_properties(suyu_libretro PROPERTIES
    PREFIX ""
    OUTPUT_NAME "suyu_libretro"
)

create_target_directory_groups(suyu_libretro)
