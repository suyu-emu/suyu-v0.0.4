cmake_minimum_required(VERSION 3.13)
project(suyu_recompiled C)
set(CMAKE_C_STANDARD 11)
add_executable(recompiled main.c recompiled.c recomp_runtime.c)
if(WIN32)
  set_target_properties(recompiled PROPERTIES OUTPUT_NAME recompiled SUFFIX ".exe")
elseif(APPLE)
  # produces a Mach-O native binary
else()
  # Linux / FreeBSD / OpenBSD: ELF
endif()
