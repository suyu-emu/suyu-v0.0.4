add_executable(tzdb2nx
    main.cpp
    tzif.cpp
    tzif.h)

set(TZDB2NX_PATH "$<TARGET_FILE:tzdb2nx>" CACHE PATH "Path to tzdb2nx path")
