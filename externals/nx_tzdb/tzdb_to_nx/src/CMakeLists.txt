add_compile_options(
    -Werror=all
    -Werror=extra

    -Werror=shadow
)

include_directories(.)

add_subdirectory(tzdb2nx)
add_subdirectory(tzdb)
