project(zlib C)

include(CheckTypeSize)
include(CheckFunctionExists)
include(CheckIncludeFile)

check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(stdint.h    HAVE_STDINT_H)
check_include_file(stddef.h    HAVE_STDDEF_H)

# Check to see if we have large file support
set(CMAKE_REQUIRED_DEFINITIONS -D_LARGEFILE64_SOURCE=1)
# We add these other definitions here because CheckTypeSize.cmake
# in CMake 2.4.x does not automatically do so and we want
# compatibility with CMake 2.4.x.
if(HAVE_SYS_TYPES_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_SYS_TYPES_H)
endif()
if(HAVE_STDINT_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_STDINT_H)
endif()
if(HAVE_STDDEF_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_STDDEF_H)
endif()
check_type_size(off64_t OFF64_T)
if(HAVE_OFF64_T)
   add_definitions(-D_LARGEFILE64_SOURCE=1)
endif()
set(CMAKE_REQUIRED_DEFINITIONS) # clear variable

# Check for fseeko
check_function_exists(fseeko HAVE_FSEEKO)
if(NOT HAVE_FSEEKO)
    add_definitions(-DNO_FSEEKO)
endif()

# Check for unistd.h
check_include_file(unistd.h HAVE_UNISTD_H)
if(HAVE_UNISTD_H)
    add_definitions(-DHAVE_UNISTD_H)
endif()

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
endif()

add_library(z STATIC
    zlib/adler32.c
    zlib/compress.c
    zlib/crc32.c
    zlib/crc32.h
    zlib/deflate.c
    zlib/deflate.h
    zlib/gzclose.c
    zlib/gzguts.h
    zlib/gzlib.c
    zlib/gzread.c
    zlib/gzwrite.c
    zlib/inffast.h
    zlib/inffixed.h
    zlib/inflate.c
    zlib/inflate.h
    zlib/infback.c
    zlib/inftrees.c
    zlib/inftrees.h
    zlib/inffast.c
    zlib/trees.c
    zlib/trees.h
    zlib/uncompr.c
    zlib/zconf.h
    zlib/zlib.h
    zlib/zutil.c
    zlib/zutil.h
)
add_library(ZLIB::ZLIB ALIAS z)

target_include_directories(z
PUBLIC
    zlib/
)
