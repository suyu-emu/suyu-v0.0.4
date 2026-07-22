# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(gpu_logging STATIC
    gpu_logging.cpp
    gpu_logging.h
    gpu_state_capture.cpp
    gpu_state_capture.h
    qualcomm_debug.cpp
    qualcomm_debug.h
)

if(ANDROID)
    target_sources(gpu_logging PRIVATE
        freedreno_debug.cpp
        freedreno_debug.h
    )
endif()

target_link_libraries(gpu_logging PUBLIC common)

if(ANDROID)
    # Link with adrenotools when available for future Qualcomm integration
    # target_link_libraries(gpu_logging PUBLIC adrenotools)
endif()
