add_executable(cpuinfo cpuinfo.cpp)
target_link_libraries(cpuinfo biscuit)
set_property(TARGET cpuinfo PROPERTY CXX_STANDARD 20)
