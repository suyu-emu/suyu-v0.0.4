# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set the RPATH for Qt Libraries
# This must be done before the `yuzu` target is created
if (YUZU_USE_BUNDLED_QT AND PLATFORM_LINUX)
    set(CMAKE_BUILD_RPATH "${CMAKE_BINARY_DIR}/bin/lib/")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(yuzu
    Info.plist
    about_dialog.cpp
    about_dialog.h
    aboutdialog.ui
    applets/qt_amiibo_settings.cpp
    applets/qt_amiibo_settings.h
    applets/qt_amiibo_settings.ui
    applets/qt_controller.cpp
    applets/qt_controller.h
    applets/qt_controller.ui
    applets/qt_error.cpp
    applets/qt_error.h
    applets/qt_profile_select.cpp
    applets/qt_profile_select.h
    applets/qt_software_keyboard.cpp
    applets/qt_software_keyboard.h
    applets/qt_software_keyboard.ui
    applets/qt_web_browser.cpp
    applets/qt_web_browser.h
    applets/qt_web_browser_scripts.h
    bootmanager.cpp
    bootmanager.h
    compatdb.ui
    compatibility_list.cpp
    compatibility_list.h
    configuration/configuration_shared.cpp
    configuration/configuration_shared.h
    configuration/configure.ui
    configuration/configure_applets.cpp
    configuration/configure_applets.h
    configuration/configure_applets.ui
    configuration/configure_audio.cpp
    configuration/configure_audio.h
    configuration/configure_audio.ui
    configuration/configure_camera.cpp
    configuration/configure_camera.h
    configuration/configure_camera.ui
    configuration/configure_cpu.cpp
    configuration/configure_cpu.h
    configuration/configure_cpu.ui
    configuration/configure_cpu_debug.cpp
    configuration/configure_cpu_debug.h
    configuration/configure_cpu_debug.ui
    configuration/configure_debug.cpp
    configuration/configure_debug.h
    configuration/configure_debug.ui
    configuration/configure_debug_controller.cpp
    configuration/configure_debug_controller.h
    configuration/configure_debug_controller.ui
    configuration/configure_debug_tab.cpp
    configuration/configure_debug_tab.h
    configuration/configure_debug_tab.ui
    configuration/configure_dialog.cpp
    configuration/configure_dialog.h
    configuration/configure_filesystem.cpp
    configuration/configure_filesystem.h
    configuration/configure_filesystem.ui
    configuration/configure_general.cpp
    configuration/configure_general.h
    configuration/configure_general.ui
    configuration/configure_graphics.cpp
    configuration/configure_graphics.h
    configuration/configure_graphics.ui
    configuration/configure_graphics_advanced.cpp
    configuration/configure_graphics_advanced.h
    configuration/configure_graphics_advanced.ui
    configuration/configure_graphics_extensions.cpp
    configuration/configure_graphics_extensions.h
    configuration/configure_graphics_extensions.ui
    configuration/configure_hotkeys.cpp
    configuration/configure_hotkeys.h
    configuration/configure_hotkeys.ui
    configuration/configure_input.cpp
    configuration/configure_input.h
    configuration/configure_input.ui
    configuration/configure_input_advanced.cpp
    configuration/configure_input_advanced.h
    configuration/configure_input_advanced.ui
    configuration/configure_input_per_game.cpp
    configuration/configure_input_per_game.h
    configuration/configure_input_per_game.ui
    configuration/configure_input_player.cpp
    configuration/configure_input_player.h
    configuration/configure_input_player.ui
    configuration/configure_input_player_widget.cpp
    configuration/configure_input_player_widget.h
    configuration/configure_input_profile_dialog.cpp
    configuration/configure_input_profile_dialog.h
    configuration/configure_input_profile_dialog.ui
    configuration/configure_linux_tab.cpp
    configuration/configure_linux_tab.h
    configuration/configure_linux_tab.ui
    configuration/configure_mouse_panning.cpp
    configuration/configure_mouse_panning.h
    configuration/configure_mouse_panning.ui
    configuration/configure_motion_touch.cpp
    configuration/configure_motion_touch.h
    configuration/configure_motion_touch.ui
    configuration/configure_per_game.cpp
    configuration/configure_per_game.h
    configuration/configure_per_game.ui
    configuration/configure_per_game_addons.cpp
    configuration/configure_per_game_addons.h
    configuration/configure_per_game_addons.ui
    configuration/configure_profile_manager.cpp
    configuration/configure_profile_manager.h
    configuration/configure_profile_manager.ui
    configuration/configure_ringcon.cpp
    configuration/configure_ringcon.h
    configuration/configure_ringcon.ui
    configuration/configure_network.cpp
    configuration/configure_network.h
    configuration/configure_network.ui
    configuration/configure_system.cpp
    configuration/configure_system.h
    configuration/configure_system.ui
    configuration/configure_tas.cpp
    configuration/configure_tas.h
    configuration/configure_tas.ui
    configuration/configure_touch_from_button.cpp
    configuration/configure_touch_from_button.h
    configuration/configure_touch_from_button.ui
    configuration/configure_touchscreen_advanced.cpp
    configuration/configure_touchscreen_advanced.h
    configuration/configure_touchscreen_advanced.ui
    configuration/configure_touch_widget.h
    configuration/configure_ui.cpp
    configuration/configure_ui.h
    configuration/configure_ui.ui
    configuration/configure_vibration.cpp
    configuration/configure_vibration.h
    configuration/configure_vibration.ui
    configuration/configure_web.cpp
    configuration/configure_web.h
    configuration/configure_web.ui
    configuration/input_profiles.cpp
    configuration/input_profiles.h
    configuration/shared_widget.cpp
    configuration/shared_widget.h
    debugger/console.cpp
    debugger/console.h
    debugger/controller.cpp
    debugger/controller.h

    game_list.cpp
    game_list.h
    game_list_p.h
    game_list_worker.cpp
    game_list_worker.h
    hotkeys.cpp
    hotkeys.h
    install_dialog.cpp
    install_dialog.h
    loading_screen.cpp
    loading_screen.h
    loading_screen.ui

    main.cpp

    multiplayer/chat_room.cpp
    multiplayer/chat_room.h
    multiplayer/chat_room.ui
    multiplayer/client_room.h
    multiplayer/client_room.cpp
    multiplayer/client_room.ui
    multiplayer/direct_connect.cpp
    multiplayer/direct_connect.h
    multiplayer/direct_connect.ui
    multiplayer/host_room.cpp
    multiplayer/host_room.h
    multiplayer/host_room.ui
    multiplayer/lobby.cpp
    multiplayer/lobby.h
    multiplayer/lobby.ui
    multiplayer/lobby_p.h
    multiplayer/message.cpp
    multiplayer/message.h
    multiplayer/moderation_dialog.cpp
    multiplayer/moderation_dialog.h
    multiplayer/moderation_dialog.ui
    multiplayer/state.cpp
    multiplayer/state.h
    multiplayer/validation.h
    startup_checks.cpp
    startup_checks.h
    set_play_time_dialog.cpp
    set_play_time_dialog.h
    util/clickable_label.cpp
    util/clickable_label.h
    util/controller_navigation.cpp
    util/controller_navigation.h
    util/limitable_input_dialog.cpp
    util/limitable_input_dialog.h
    util/overlay_dialog.cpp
    util/overlay_dialog.h
    util/overlay_dialog.ui
    util/sequence_dialog/sequence_dialog.cpp
    util/sequence_dialog/sequence_dialog.h
    util/url_request_interceptor.cpp
    util/url_request_interceptor.h
    util/util.cpp
    util/util.h
    vk_device_info.cpp
    vk_device_info.h
    compatdb.cpp
    compatdb.h
    user_data_migration.cpp
    user_data_migration.h

    yuzu.qrc
    yuzu.rc
    migration_dialog.h migration_dialog.cpp
    migration_worker.h
    migration_worker.cpp

    deps_dialog.cpp
    deps_dialog.h
    deps_dialog.ui

    data_dialog.h data_dialog.cpp data_dialog.ui
    data_widget.ui
    ryujinx_dialog.h ryujinx_dialog.cpp ryujinx_dialog.ui
    main_window.h main_window.cpp
)

set_target_properties(yuzu PROPERTIES OUTPUT_NAME "eden")

if (YUZU_CRASH_DUMPS)
    target_sources(yuzu PRIVATE
        breakpad.cpp
        breakpad.h
    )

    target_link_libraries(yuzu PRIVATE libbreakpad_client)
    target_compile_definitions(yuzu PRIVATE YUZU_CRASH_DUMPS)
endif()

if (CXX_CLANG)
    target_compile_definitions(yuzu PRIVATE
        $<$<VERSION_LESS:$<CXX_COMPILER_VERSION>,15>:CANNOT_EXPLICITLY_INSTANTIATE>
    )
endif()

file(GLOB COMPAT_LIST
     ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.qrc
     ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json)
file(GLOB_RECURSE ICONS ${PROJECT_SOURCE_DIR}/dist/icons/*)
file(GLOB_RECURSE THEMES ${PROJECT_SOURCE_DIR}/dist/qt_themes/*)

if (ENABLE_UPDATE_CHECKER)
    target_compile_definitions(yuzu PUBLIC ENABLE_UPDATE_CHECKER)
endif()

if (ENABLE_QT_TRANSLATION)
    set(YUZU_QT_LANGUAGES "${PROJECT_SOURCE_DIR}/dist/languages" CACHE PATH "Path to the translation bundle for the Qt frontend")
    option(GENERATE_QT_TRANSLATION "Generate en.ts as the translation source file" OFF)
    option(WORKAROUND_BROKEN_LUPDATE "Run lupdate directly through CMake if Qt's convenience wrappers don't work" OFF)

    # Update source TS file if enabled
    if (GENERATE_QT_TRANSLATION)
        get_target_property(SRCS yuzu SOURCES)
        # these calls to qt_create_translation also creates a rule to generate en.qm which conflicts with providing english plurals
        # so we have to set a OUTPUT_LOCATION so that we don't have multiple rules to generate en.qm
        set_source_files_properties(${YUZU_QT_LANGUAGES}/en.ts PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/translations")
        if (WORKAROUND_BROKEN_LUPDATE)
            add_custom_command(OUTPUT ${YUZU_QT_LANGUAGES}/en.ts
                COMMAND lupdate
                    -source-language en_US
                    -target-language en_US
                    ${SRCS}
                    ${UIS}
                    -ts ${YUZU_QT_LANGUAGES}/en.ts
                DEPENDS
                    ${SRCS}
                    ${UIS}
                WORKING_DIRECTORY
                    ${CMAKE_CURRENT_SOURCE_DIR}
            )
        else()
            qt_create_translation(QM_FILES
                ${SRCS}
                ${UIS}
                ${YUZU_QT_LANGUAGES}/en.ts
            OPTIONS
                -source-language en_US
                -target-language en_US
            )
        endif()

        # Generate plurals into dist/english_plurals/generated_en.ts so it can be used to revise dist/english_plurals/en.ts
        set(GENERATED_PLURALS_FILE ${PROJECT_SOURCE_DIR}/dist/english_plurals/generated_en.ts)
        set_source_files_properties(${GENERATED_PLURALS_FILE} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/plurals")
        if (WORKAROUND_BROKEN_LUPDATE)
            add_custom_command(OUTPUT ${GENERATED_PLURALS_FILE}
                COMMAND lupdate
                    -source-language en_US
                    -target-language en_US
                    ${SRCS}
                    ${UIS}
                    -ts ${GENERATED_PLURALS_FILE}
                DEPENDS
                    ${SRCS}
                    ${UIS}
                WORKING_DIRECTORY
                    ${CMAKE_CURRENT_SOURCE_DIR}
            )
        else()
            qt_create_translation(QM_FILES ${SRCS} ${UIS} ${GENERATED_PLURALS_FILE} OPTIONS -pluralonly -source-language en_US -target-language en_US)
        endif()

        add_custom_target(translation ALL DEPENDS ${YUZU_QT_LANGUAGES}/en.ts ${GENERATED_PLURALS_FILE})
    endif()

    # Find all TS files except en.ts
    file(GLOB_RECURSE LANGUAGES_TS ${YUZU_QT_LANGUAGES}/*.ts)
    list(REMOVE_ITEM LANGUAGES_TS ${YUZU_QT_LANGUAGES}/en.ts)

    # Compile TS files to QM files
    qt_add_translation(LANGUAGES_QM ${LANGUAGES_TS})

    # Compile english plurals TS file to en.qm
    qt_add_translation(LANGUAGES_QM ${PROJECT_SOURCE_DIR}/dist/english_plurals/en.ts)

    # Build a QRC file from the QM file list
    set(LANGUAGES_QRC ${CMAKE_CURRENT_BINARY_DIR}/languages.qrc)
    file(WRITE ${LANGUAGES_QRC} "<RCC><qresource prefix=\"languages\">\n")
    foreach (QM ${LANGUAGES_QM})
        get_filename_component(QM_FILE ${QM} NAME)
        file(APPEND ${LANGUAGES_QRC} "<file>${QM_FILE}</file>\n")
    endforeach (QM)
    file(APPEND ${LANGUAGES_QRC} "</qresource></RCC>")

    # Add the QRC file to package in all QM files
    qt_add_resources(LANGUAGES ${LANGUAGES_QRC})
else()
    set(LANGUAGES)
endif()

target_sources(yuzu
    PRIVATE
        ${COMPAT_LIST}
        ${ICONS}
        ${LANGUAGES}
        ${THEMES}
)

if (APPLE)
    set(MACOSX_ICON "../../dist/eden.icns")
    set_source_files_properties(${MACOSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(yuzu PRIVATE ${MACOSX_ICON})
    set_target_properties(yuzu PROPERTIES MACOSX_BUNDLE TRUE)
    set_target_properties(yuzu PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)

    if (YUZU_USE_BUNDLED_MOLTENVK)
        set(MOLTENVK_PLATFORM "macOS")
        set(MOLTENVK_VERSION "v1.4.0")
        download_moltenvk(${MOLTENVK_PLATFORM} ${MOLTENVK_VERSION})
    endif()

    set(CMAKE_FIND_LIBRARY_SUFFIXES ".dylib")
    find_library(MOLTENVK_LIBRARY MoltenVK REQUIRED)
    message(STATUS "Using MoltenVK at ${MOLTENVK_LIBRARY}.")

    set_source_files_properties(${MOLTENVK_LIBRARY} PROPERTIES
        MACOSX_PACKAGE_LOCATION Frameworks
        XCODE_FILE_ATTRIBUTES "CodeSignOnCopy")
    target_sources(yuzu PRIVATE ${MOLTENVK_LIBRARY})
elseif(WIN32)
    # compile as a win32 gui application instead of a console application
    target_link_libraries(yuzu PRIVATE Qt6::EntryPointPrivate)
    if(MSVC)
        target_link_libraries(yuzu PRIVATE version.lib)
        set_target_properties(yuzu PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
    elseif(MINGW)
        set_target_properties(yuzu PROPERTIES LINK_FLAGS_RELEASE "-Wl,--subsystem,windows")
        target_link_libraries(yuzu PRIVATE dwmapi)
    endif()
endif()

target_link_libraries(yuzu PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(yuzu PRIVATE common core input_common frontend_common network video_core qt_common)
target_link_libraries(yuzu PRIVATE Boost::headers glad Qt6::Widgets Qt6::Concurrent)
target_link_libraries(yuzu PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

if (UNIX AND NOT APPLE)
    target_link_libraries(yuzu PRIVATE Qt6::DBus)
endif()

if (YUZU_ENABLE_COMPATIBILITY_REPORTING)
    target_compile_definitions(yuzu PRIVATE YUZU_ENABLE_COMPATIBILITY_REPORTING)
endif()

if (ENABLE_WEB_SERVICE)
    target_compile_definitions(yuzu PRIVATE ENABLE_WEB_SERVICE)
endif()

if (YUZU_USE_QT_MULTIMEDIA)
    target_link_libraries(yuzu PRIVATE Qt6::Multimedia)
    target_compile_definitions(yuzu PRIVATE YUZU_USE_QT_MULTIMEDIA)
endif ()

if (YUZU_USE_QT_WEB_ENGINE)
    target_link_libraries(yuzu PRIVATE Qt6::WebEngineCore Qt6::WebEngineWidgets)
    target_compile_definitions(yuzu PRIVATE YUZU_USE_QT_WEB_ENGINE)
endif ()

if(UNIX AND NOT APPLE)
    install(TARGETS yuzu)
endif()

if (WIN32 AND NOT YUZU_USE_BUNDLED_QT AND QT_VERSION VERSION_GREATER_EQUAL 6)
    set(YUZU_EXE_DIR "$<TARGET_FILE_DIR:yuzu>")
    add_custom_command(TARGET yuzu POST_BUILD COMMAND ${WINDEPLOYQT_EXECUTABLE} "${YUZU_EXE_DIR}/eden.exe" --dir "${YUZU_EXE_DIR}" --libdir "${YUZU_EXE_DIR}" --plugindir "${YUZU_EXE_DIR}/plugins" --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler --no-translations --verbose 0)
endif()

# TODO(crueter): this can be done with system qt in a better way
if (YUZU_USE_BUNDLED_QT)
    include(CopyYuzuQt6Deps)
    copy_yuzu_Qt6_deps(yuzu)
endif()

if (ENABLE_SDL2)
    target_link_libraries(yuzu PRIVATE SDL2::SDL2)
    target_compile_definitions(yuzu PRIVATE HAVE_SDL2)
endif()

if (ARCHITECTURE_x86_64 OR ARCHITECTURE_arm64)
    target_link_libraries(yuzu PRIVATE dynarmic::dynarmic)
endif()

if (YUZU_ROOM)
    target_link_libraries(yuzu PRIVATE yuzu-room)
target_link_libraries(yuzu PRIVATE Qt6::Widgets)
endif()

if (NOT MSVC AND (APPLE OR NOT YUZU_STATIC_BUILD))
    # needed for vma
    target_compile_options(yuzu PRIVATE
        -Wno-conversion
        -Wno-unused-variable
        -Wno-unused-parameter
        -Wno-missing-field-initializers)
endif()

if (YUZU_STATIC_BUILD AND MINGW)
    static_qt_link(yuzu)
endif()

create_target_directory_groups(yuzu)
