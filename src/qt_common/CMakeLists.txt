# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(qt_common STATIC
    qt_common.h
    qt_common.cpp

    uisettings.cpp
    uisettings.h

    qt_config.cpp
    qt_config.h

    shared_translation.cpp
    shared_translation.h
    qt_path_util.h qt_path_util.cpp
    qt_game_util.h qt_game_util.cpp
    qt_frontend_util.h qt_frontend_util.cpp
    qt_meta.h qt_meta.cpp
    qt_content_util.h qt_content_util.cpp
    qt_rom_util.h qt_rom_util.cpp
    qt_applet_util.h qt_applet_util.cpp
    qt_progress_dialog.h qt_progress_dialog.cpp

)

create_target_directory_groups(qt_common)

# TODO(crueter)
if (ENABLE_QT)
    target_link_libraries(qt_common PRIVATE Qt6::Widgets)
endif()

add_subdirectory(externals)

target_link_libraries(qt_common PRIVATE core Qt6::Core SimpleIni::SimpleIni QuaZip::QuaZip frozen::frozen)
target_link_libraries(qt_common PRIVATE Qt6::Core)

if (NOT WIN32)
    target_include_directories(qt_common PRIVATE ${Qt6Gui_PRIVATE_INCLUDE_DIRS})
endif()
