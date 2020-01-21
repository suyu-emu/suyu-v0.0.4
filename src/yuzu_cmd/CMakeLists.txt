set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

add_executable(yuzu-cmd
    config.cpp
    config.h
    default_ini.h
    emu_window/emu_window_sdl2_gl.cpp
    emu_window/emu_window_sdl2_gl.h
    emu_window/emu_window_sdl2.cpp
    emu_window/emu_window_sdl2.h
    emu_window/emu_window_sdl2_gl.cpp
    emu_window/emu_window_sdl2_gl.h
    resource.h
    yuzu.cpp
    yuzu.rc
)

if (ENABLE_VULKAN)
    target_sources(yuzu-cmd PRIVATE
                   emu_window/emu_window_sdl2_vk.cpp
                   emu_window/emu_window_sdl2_vk.h)

    target_include_directories(yuzu-cmd PRIVATE ../../externals/Vulkan-Headers/include)
    target_compile_definitions(yuzu-cmd PRIVATE HAS_VULKAN)
endif()

create_target_directory_groups(yuzu-cmd)

target_link_libraries(yuzu-cmd PRIVATE common core input_common)
target_link_libraries(yuzu-cmd PRIVATE inih glad)
if (MSVC)
    target_link_libraries(yuzu-cmd PRIVATE getopt)
endif()
target_link_libraries(yuzu-cmd PRIVATE ${PLATFORM_LIBRARIES} SDL2 Threads::Threads)

if(UNIX AND NOT APPLE)
    install(TARGETS yuzu-cmd RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (MSVC)
    include(CopyYuzuSDLDeps)
    include(CopyYuzuUnicornDeps)
    copy_yuzu_SDL_deps(yuzu-cmd)
    copy_yuzu_unicorn_deps(yuzu-cmd)
endif()
