# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(qt_common STATIC
    qt_common.h
    qt_common.cpp

    config/uisettings.cpp
    config/uisettings.h
    config/qt_config.cpp
    config/qt_config.h
    config/shared_translation.cpp
    config/shared_translation.h

    util/path.h util/path.cpp
    util/game.h util/game.cpp
    util/meta.h util/meta.cpp
    util/content.h util/content.cpp
    util/rom.h util/rom.cpp
    util/applet.h util/applet.cpp
    util/compress.h util/compress.cpp

    abstract/frontend.h abstract/frontend.cpp
    abstract/qt_progress_dialog.h abstract/qt_progress_dialog.cpp

    qt_string_lookup.h
    qt_compat.h

    discord/discord.h
    util/fs.h util/fs.cpp
)

if (UNIX)
    target_sources(qt_common PRIVATE gui_settings.cpp gui_settings.h)
endif()

create_target_directory_groups(qt_common)

if (USE_DISCORD_PRESENCE)
    target_sources(qt_common PRIVATE
        discord/discord_impl.cpp
        discord/discord_impl.h
    )
    target_link_libraries(qt_common PUBLIC DiscordRPC::discord-rpc httplib::httplib)

    if (YUZU_USE_BUNDLED_OPENSSL)
        target_link_libraries(qt_common PUBLIC OpenSSL::SSL OpenSSL::Crypto)
        target_compile_definitions(qt_common PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
    endif()

    target_compile_definitions(qt_common PUBLIC USE_DISCORD_PRESENCE)
endif()

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

# pass targets
find_package(frozen)

target_link_libraries(qt_common PRIVATE core Qt6::Core Qt6::Concurrent SimpleIni::SimpleIni QuaZip::QuaZip)
target_link_libraries(qt_common PUBLIC frozen::frozen-headers)

if (NOT APPLE AND ENABLE_OPENGL)
    target_compile_definitions(qt_common PUBLIC HAS_OPENGL)
endif()

if (UNIX AND NOT APPLE)
    if (DEFINED Qt6Gui_PRIVATE_INCLUDE_DIRS)
        target_include_directories(qt_common PRIVATE ${Qt6Gui_PRIVATE_INCLUDE_DIRS})
    else()
        target_link_libraries(qt_common PRIVATE Qt6::GuiPrivate)
    endif()
endif()
