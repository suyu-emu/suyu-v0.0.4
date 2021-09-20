add_library(input_common STATIC
    drivers/keyboard.cpp
    drivers/keyboard.h
    drivers/mouse.cpp
    drivers/mouse.h
    drivers/touch_screen.cpp
    drivers/touch_screen.h
    helpers/stick_from_buttons.cpp
    helpers/stick_from_buttons.h
    helpers/touch_from_buttons.cpp
    helpers/touch_from_buttons.h
    helpers/udp_protocol.cpp
    helpers/udp_protocol.h
    input_engine.cpp
    input_engine.h
    input_mapping.cpp
    input_mapping.h
    input_poller.cpp
    input_poller.h
    main.cpp
    main.h
    motion_from_button.cpp
    motion_from_button.h
    motion_input.cpp
    motion_input.h
    gcadapter/gc_adapter.cpp
    gcadapter/gc_adapter.h
    gcadapter/gc_poller.cpp
    gcadapter/gc_poller.h
    sdl/sdl.cpp
    sdl/sdl.h
    tas/tas_input.cpp
    tas/tas_input.h
    tas/tas_poller.cpp
    tas/tas_poller.h
    udp/client.cpp
    udp/client.h
    udp/udp.cpp
    udp/udp.h
)

if (MSVC)
    target_compile_options(input_common PRIVATE
        /W4
        /WX

        /we4242 # 'identifier': conversion from 'type1' to 'type2', possible loss of data
        /we4244 # 'conversion': conversion from 'type1' to 'type2', possible loss of data
        /we4245 # 'conversion': conversion from 'type1' to 'type2', signed/unsigned mismatch
        /we4254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
    )
else()
    target_compile_options(input_common PRIVATE
        -Werror
        -Werror=conversion
        -Werror=ignored-qualifiers
        -Werror=shadow
        $<$<CXX_COMPILER_ID:GNU>:-Werror=unused-but-set-parameter>
        $<$<CXX_COMPILER_ID:GNU>:-Werror=unused-but-set-variable>
        -Werror=unused-variable
    )
endif()

if (ENABLE_SDL2)
    target_sources(input_common PRIVATE
        sdl/sdl_impl.cpp
        sdl/sdl_impl.h
    )
    target_link_libraries(input_common PRIVATE SDL2)
    target_compile_definitions(input_common PRIVATE HAVE_SDL2)
endif()

target_link_libraries(input_common PRIVATE usb)

create_target_directory_groups(input_common)
target_link_libraries(input_common PUBLIC core PRIVATE common Boost::boost)
