# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

## When linking to a static Qt build on MinGW, certain additional libraries
## must be statically linked to as well.

function(static_qt_link target)
    macro(extra_libs)
        foreach(lib ${ARGN})
            find_library(${lib}_LIBRARY ${lib} REQUIRED)
            target_link_libraries(${target} PRIVATE ${${lib}_LIBRARY})
        endforeach()
    endmacro()

    # I am constantly impressed at how ridiculously stupid the linker is
    # NB: yes, we have to put them here twice. I have no idea why

    # libtiff.a
    extra_libs(tiff jbig bz2 lzma deflate jpeg tiff)

    # libfreetype.a
    extra_libs(freetype bz2 Lerc brotlidec brotlicommon freetype)

    # libharfbuzz.a
    extra_libs(harfbuzz graphite2)

    # sijfjkfnjkdfjsbjsbsdfhvbdf
    if (ENABLE_OPENSSL)
        target_link_libraries(${target} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
endfunction()
