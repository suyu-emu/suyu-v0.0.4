add_library(input_common STATIC
    analog_from_button.cpp
    analog_from_button.h
    keyboard.cpp
    keyboard.h
    main.cpp
    main.h
    motion_emu.cpp
    motion_emu.h
    motion_input.cpp
    motion_input.h
    settings.cpp
    settings.h
    touch_from_button.cpp
    touch_from_button.h
    gcadapter/gc_adapter.cpp
    gcadapter/gc_adapter.h
    gcadapter/gc_poller.cpp
    gcadapter/gc_poller.h
    sdl/sdl.cpp
    sdl/sdl.h
    udp/client.cpp
    udp/client.h
    udp/protocol.cpp
    udp/protocol.h
    udp/udp.cpp
    udp/udp.h
)

if(SDL2_FOUND)
    target_sources(input_common PRIVATE
        sdl/sdl_impl.cpp
        sdl/sdl_impl.h
    )
    target_link_libraries(input_common PRIVATE SDL2)
    target_compile_definitions(input_common PRIVATE HAVE_SDL2)
endif()

target_include_directories(input_common SYSTEM PRIVATE ${LIBUSB_INCLUDE_DIR})
target_link_libraries(input_common PRIVATE ${LIBUSB_LIBRARIES})

create_target_directory_groups(input_common)
target_link_libraries(input_common PUBLIC core PRIVATE common Boost::boost)
