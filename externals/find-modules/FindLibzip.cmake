
find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBZIP QUIET libzip)

find_path(LIBZIP_INCLUDE_DIR
  NAMES zip.h
  PATHS ${PC_LIBZIP_INCLUDE_DIRS}
  "$ENV{LIB_DIR}/include"
  "$ENV{INCLUDE}"
  /usr/local/include
  /usr/include
)
find_path(LIBZIP_INCLUDE_DIR_ZIPCONF
  NAMES zipconf.h
  HINTS ${PC_LIBZIP_INCLUDE_DIRS}
  "$ENV{LIB_DIR}/include"
  "$ENV{LIB_DIR}/lib/libzip/include"
  "$ENV{LIB}/lib/libzip/include"
  /usr/local/lib/libzip/include
  /usr/lib/libzip/include
  /usr/local/include
  /usr/include
  "$ENV{INCLUDE}"
)
find_library(LIBZIP_LIBRARY
  NAMES zip
  PATHS ${PC_LIBZIP_LIBRARY_DIRS}
  "$ENV{LIB_DIR}/lib" "$ENV{LIB}" /usr/local/lib /usr/lib
)

if (LIBZIP_INCLUDE_DIR_ZIPCONF)
  FILE(READ "${LIBZIP_INCLUDE_DIR_ZIPCONF}/zipconf.h" _LIBZIP_VERSION_CONTENTS)
  if (_LIBZIP_VERSION_CONTENTS)
    STRING(REGEX REPLACE ".*#define LIBZIP_VERSION \"([0-9.]+)\".*" "\\1" LIBZIP_VERSION "${_LIBZIP_VERSION_CONTENTS}")
  endif()
  unset(_LIBZIP_VERSION_CONTENTS)
endif()

set(LIBZIP_VERSION ${LIBZIP_VERSION} CACHE STRING "Version number of libzip")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libzip
  FOUND_VAR LIBZIP_FOUND
  REQUIRED_VARS
    LIBZIP_LIBRARY
    LIBZIP_INCLUDE_DIR
    LIBZIP_INCLUDE_DIR_ZIPCONF
    LIBZIP_VERSION
  VERSION_VAR LIBZIP_VERSION
)

if(LIBZIP_FOUND)
  set(LIBZIP_LIBRARIES ${LIBZIP_LIBRARY})
  set(LIBZIP_INCLUDE_DIRS ${LIBZIP_INCLUDE_DIR})
  set(LIBZIP_DEFINITIONS ${PC_LIBZIP_CFLAGS_OTHER})
endif()

if(LIBZIP_FOUND AND NOT TARGET libzip::libzip)
  add_library(libzip::libzip UNKNOWN IMPORTED)
  set_target_properties(libzip::libzip PROPERTIES
    IMPORTED_LOCATION "${LIBZIP_LIBRARY}"
    INTERFACE_COMPILE_OPTIONS "${PC_LIBZIP_CFLAGS_OTHER}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBZIP_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(
    LIBZIP_INCLUDE_DIR
    LIBZIP_INCLUDE_DIR_ZIPCONF
    LIBZIP_LIBRARY
    LIBZIP_VERSION
)
