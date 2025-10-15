# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

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
    qt_string_lookup.h
    qt_compress.h qt_compress.cpp

)

create_target_directory_groups(qt_common)

# TODO(crueter)
if (ENABLE_QT)
    target_link_libraries(qt_common PRIVATE Qt6::Widgets)
endif()

target_compile_definitions(qt_common PUBLIC
    # Use QStringBuilder for string concatenation to reduce
    # the overall number of temporary strings created.
    QT_USE_QSTRINGBUILDER

    # Disable implicit conversions from/to C strings
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII

    # Disable implicit type narrowing in signal/slot connect() calls.
    QT_NO_NARROWING_CONVERSIONS_IN_CONNECT

    # Disable unsafe overloads of QProcess' start() function.
    QT_NO_PROCESS_COMBINED_ARGUMENT_START

    # Disable implicit QString->QUrl conversions to enforce use of proper resolving functions.
    QT_NO_URL_CAST_FROM_STRING
)

add_subdirectory(externals)

target_link_libraries(qt_common PRIVATE core Qt6::Core Qt6::Concurrent SimpleIni::SimpleIni QuaZip::QuaZip)
target_link_libraries(qt_common PUBLIC frozen::frozen)

if (NOT APPLE AND ENABLE_OPENGL)
    target_compile_definitions(qt_common PUBLIC HAS_OPENGL)
endif()

if (UNIX AND NOT APPLE)
    if (TARGET Qt6::GuiPrivate)
        target_link_libraries(qt_common PRIVATE Qt6::GuiPrivate)
    else()
        target_include_directories(qt_common PRIVATE ${Qt6Gui_PRIVATE_INCLUDE_DIRS})
    endif()
endif()
