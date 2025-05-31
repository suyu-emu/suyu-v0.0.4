cmake_minimum_required(VERSION 3.10)

project(tzdb2nx VERSION 1.0)

option(TZDB2NX_ZONEINFO_DIR "Specify a custom zoneinfo directory containing time zone data you wish to use" "")
option(TZDB2NX_VERSION "Specify a custom zoneinfo version with the directory" "")

if (TZDB2NX_ZONEINFO_DIR AND NOT TZDB2NX_VERSION)
    message(FATAL_ERROR "TZDB2NX_ZONEINFO_DIR was specified but TZDB2NX_VERSION was left undefined.")
endif()

set(CMAKE_CXX_STANDARD 20)

if (APPLE)
    find_package(Intl REQUIRED)
endif()

add_subdirectory(externals)
add_subdirectory(src)
