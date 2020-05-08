
find_package(PkgConfig QUIET)
pkg_check_modules(PC_Catch2 QUIET Catch2)

find_path(Catch2_INCLUDE_DIR
  NAMES catch.hpp
  PATHS ${PC_Catch2_INCLUDE_DIRS} ${CONAN_CATCH2_ROOT}
  PATH_SUFFIXES catch2
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Catch2
  FOUND_VAR Catch2_FOUND
  REQUIRED_VARS
    Catch2_INCLUDE_DIR
  VERSION_VAR Catch2_VERSION
)

if(Catch2_FOUND)
  set(Catch2_INCLUDE_DIRS ${Catch2_INCLUDE_DIR})
  set(Catch2_DEFINITIONS ${PC_Catch2_CFLAGS_OTHER})
endif()

if(Catch2_FOUND AND NOT TARGET Catch2::Catch2)
  add_library(Catch2::Catch2 UNKNOWN IMPORTED)
  set_target_properties(Catch2::Catch2 PROPERTIES
    IMPORTED_LOCATION "${Catch2_LIBRARY}"
    INTERFACE_COMPILE_OPTIONS "${PC_Catch2_CFLAGS_OTHER}"
    INTERFACE_INCLUDE_DIRECTORIES "${Catch2_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(
    Catch2_INCLUDE_DIR
)
