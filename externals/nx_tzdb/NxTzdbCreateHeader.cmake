# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

set(ZONE_PATH ${CMAKE_ARGV3})
set(HEADER_NAME ${CMAKE_ARGV4})
set(NX_TZDB_INCLUDE_DIR ${CMAKE_ARGV5})
set(NX_TZDB_SOURCE_DIR ${CMAKE_ARGV6})

execute_process(
    COMMAND ${CMAKE_COMMAND} -P ${NX_TZDB_SOURCE_DIR}/ListFilesInDirectory.cmake
    WORKING_DIRECTORY ${ZONE_PATH}
    OUTPUT_VARIABLE FILE_LIST)

if (NOT FILE_LIST)
    message(FATAL_ERROR "No timezone files found in directory ${ZONE_PATH}, did the download fail?")
endif()

set(DIRECTORY_NAME ${HEADER_NAME})
set(FILE_DATA "")

string(APPEND FILE_DATA "[[nodiscard]] static inline std::vector<FileSys::VirtualFile> CollectFiles_${DIRECTORY_NAME}() {\n")
string(APPEND FILE_DATA [[
    std::vector<FileSys::VirtualFile> vfs_files;
    auto const fn = [&](std::string_view name, std::span<const u8> data) {
        vfs_files.push_back(std::make_shared<FileSys::VectorVfsFile>(
            std::vector<u8>(data.begin(), data.end()),
            std::string{name}
        ));
    };
]])
foreach(ZONE_FILE ${FILE_LIST})
    if (ZONE_FILE STREQUAL "\n")
        continue()
    endif()
    string(APPEND FILE_DATA "  {\n")
    string(APPEND FILE_DATA "    constexpr uint8_t tzdb_data[] = {\n")
    file(READ ${ZONE_PATH}/${ZONE_FILE} ZONE_DATA HEX)
    string(LENGTH "${ZONE_DATA}" ZONE_DATA_LEN)
    foreach(I RANGE 0 ${ZONE_DATA_LEN} 2)
        math(EXPR BREAK_LINE "(${I} + 2) % 38")

        string(SUBSTRING "${ZONE_DATA}" "${I}" 2 HEX_DATA)
        if (NOT HEX_DATA)
            break()
        endif()

        string(APPEND FILE_DATA "0x${HEX_DATA},")
        if (BREAK_LINE EQUAL 0)
            string(APPEND FILE_DATA "\n")
        else()
            string(APPEND FILE_DATA " ")
        endif()
    endforeach()
    string(APPEND FILE_DATA "    };\n")
    string(APPEND FILE_DATA "    fn(\"${ZONE_FILE}\", tzdb_data);\n")
    string(APPEND FILE_DATA "  }\n")
endforeach()
string(APPEND FILE_DATA "  return vfs_files;\n")
string(APPEND FILE_DATA "}\n")

file(READ ${NX_TZDB_SOURCE_DIR}/tzdb_template.h.in NX_TZDB_TEMPLATE_H_IN)
file(CONFIGURE OUTPUT ${NX_TZDB_INCLUDE_DIR}/nx_tzdb/${HEADER_NAME}.h CONTENT "${NX_TZDB_TEMPLATE_H_IN}")
