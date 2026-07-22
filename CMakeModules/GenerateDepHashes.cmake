# SPDX-FileCopyrightText: 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

get_property(NAMES GLOBAL PROPERTY CPM_PACKAGE_NAMES)
get_property(SHAS GLOBAL PROPERTY CPM_PACKAGE_SHAS)
get_property(URLS GLOBAL PROPERTY CPM_PACKAGE_URLS)

list(LENGTH NAMES DEPS_LENGTH)

list(JOIN NAMES "\",\n\t\"" DEP_NAME_DIRTY)
set(DEP_NAMES "\t\"${DEP_NAME_DIRTY}\"")

list(JOIN SHAS "\",\n\t\"" DEP_SHAS_DIRTY)
set(DEP_SHAS "\t\"${DEP_SHAS_DIRTY}\"")

list(JOIN URLS "\",\n\t\"" DEP_URLS_DIRTY)
set(DEP_URLS "\t\"${DEP_URLS_DIRTY}\"")

configure_file(dep_hashes.h.in dep_hashes.h @ONLY)
target_sources(common PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/dep_hashes.h)
target_include_directories(common PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
