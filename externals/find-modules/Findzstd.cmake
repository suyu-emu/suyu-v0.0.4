
find_package(PkgConfig QUIET)
pkg_check_modules(PC_zstd QUIET libzstd)

find_path(zstd_INCLUDE_DIR
  NAMES zstd.h
  PATHS ${PC_zstd_INCLUDE_DIRS}
)
find_library(zstd_LIBRARY
  NAMES zstd
  PATHS ${PC_zstd_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(zstd
  FOUND_VAR zstd_FOUND
  REQUIRED_VARS
    zstd_LIBRARY
    zstd_INCLUDE_DIR
  VERSION_VAR zstd_VERSION
)

if(zstd_FOUND)
  set(zstd_LIBRARIES ${zstd_LIBRARY})
  set(zstd_INCLUDE_DIRS ${zstd_INCLUDE_DIR})
  set(zstd_DEFINITIONS ${PC_zstd_CFLAGS_OTHER})
endif()

if(zstd_FOUND AND NOT TARGET zstd::zstd)
  add_library(zstd::zstd UNKNOWN IMPORTED)
  set_target_properties(zstd::zstd PROPERTIES
    IMPORTED_LOCATION "${zstd_LIBRARY}"
    INTERFACE_COMPILE_OPTIONS "${PC_zstd_CFLAGS_OTHER}"
    INTERFACE_INCLUDE_DIRECTORIES "${zstd_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(
    zstd_INCLUDE_DIR
    zstd_LIBRARY
)
