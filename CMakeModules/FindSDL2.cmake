# FindSDL2.cmake - Find SDL2 library
# Sets SDL2_FOUND, SDL2_INCLUDE_DIRS, SDL2_LIBRARIES, and creates SDL2::SDL2 target

include(FindPackageHandleStandardArgs)

find_path(SDL2_INCLUDE_DIR SDL.h
    HINTS
        $ENV{SDL2DIR}
        ${SDL2_ROOT}
    PATH_SUFFIXES include/SDL2 include
)

find_library(SDL2_LIBRARY
    NAMES SDL2
    HINTS
        $ENV{SDL2DIR}
        ${SDL2_ROOT}
    PATH_SUFFIXES lib
)

find_library(SDL2MAIN_LIBRARY
    NAMES SDL2main
    HINTS
        $ENV{SDL2DIR}
        ${SDL2_ROOT}
    PATH_SUFFIXES lib
)

if(SDL2_INCLUDE_DIR AND EXISTS "${SDL2_INCLUDE_DIR}/SDL_version.h")
    file(STRINGS "${SDL2_INCLUDE_DIR}/SDL_version.h" SDL2_VERSION_MAJOR_LINE REGEX "^#define[ \t]+SDL_MAJOR_VERSION[ \t]+[0-9]+$")
    file(STRINGS "${SDL2_INCLUDE_DIR}/SDL_version.h" SDL2_VERSION_MINOR_LINE REGEX "^#define[ \t]+SDL_MINOR_VERSION[ \t]+[0-9]+$")
    file(STRINGS "${SDL2_INCLUDE_DIR}/SDL_version.h" SDL2_VERSION_PATCH_LINE REGEX "^#define[ \t]+SDL_PATCHLEVEL[ \t]+[0-9]+$")
    string(REGEX REPLACE "^#define[ \t]+SDL_MAJOR_VERSION[ \t]+([0-9]+)$" "\\1" SDL2_VERSION_MAJOR "${SDL2_VERSION_MAJOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+SDL_MINOR_VERSION[ \t]+([0-9]+)$" "\\1" SDL2_VERSION_MINOR "${SDL2_VERSION_MINOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+SDL_PATCHLEVEL[ \t]+([0-9]+)$" "\\1" SDL2_VERSION_PATCH "${SDL2_VERSION_PATCH_LINE}")
    set(SDL2_VERSION_STRING "${SDL2_VERSION_MAJOR}.${SDL2_VERSION_MINOR}.${SDL2_VERSION_PATCH}")
endif()

find_package_handle_standard_args(SDL2
    REQUIRED_VARS SDL2_LIBRARY SDL2_INCLUDE_DIR
    VERSION_VAR SDL2_VERSION_STRING
)

if(SDL2_FOUND AND NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 UNKNOWN IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${SDL2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
    )
    if(SDL2MAIN_LIBRARY)
        add_library(SDL2::SDL2main UNKNOWN IMPORTED)
        set_target_properties(SDL2::SDL2main PROPERTIES
            IMPORTED_LOCATION "${SDL2MAIN_LIBRARY}"
        )
    endif()
endif()

set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
set(SDL2_LIBRARIES ${SDL2_LIBRARY})
