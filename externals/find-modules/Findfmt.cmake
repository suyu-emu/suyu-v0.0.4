
find_package(PkgConfig QUIET)
pkg_check_modules(PC_fmt QUIET fmt)

find_path(fmt_INCLUDE_DIR
  NAMES format.h
  PATHS ${PC_fmt_INCLUDE_DIRS} ${CONAN_INCLUDE_DIRS_fmt}
  PATH_SUFFIXES fmt
)

find_library(fmt_LIBRARY
  NAMES fmt
  PATHS ${PC_fmt_LIBRARY_DIRS} ${CONAN_LIB_DIRS_fmt}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(fmt
  FOUND_VAR fmt_FOUND
  REQUIRED_VARS
    fmt_LIBRARY
    fmt_INCLUDE_DIR
  VERSION_VAR fmt_VERSION
)

if(fmt_FOUND)
  set(fmt_LIBRARIES ${fmt_LIBRARY})
  set(fmt_INCLUDE_DIRS ${fmt_INCLUDE_DIR})
  set(fmt_DEFINITIONS ${PC_fmt_CFLAGS_OTHER})
endif()

if(fmt_FOUND AND NOT TARGET fmt::fmt)
  add_library(fmt::fmt UNKNOWN IMPORTED)
  set_target_properties(fmt::fmt PROPERTIES
    IMPORTED_LOCATION "${fmt_LIBRARY}"
    INTERFACE_COMPILE_OPTIONS "${PC_fmt_CFLAGS_OTHER}"
    INTERFACE_INCLUDE_DIRECTORIES "${fmt_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(
    fmt_INCLUDE_DIR
    fmt_LIBRARY
)
