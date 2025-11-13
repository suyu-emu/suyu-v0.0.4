# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2021 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

include(CPMUtil)

# TODO(crueter, MaranBr): Externals FFmpeg 8.0

set(FFmpeg_HWACCEL_LIBRARIES)
set(FFmpeg_HWACCEL_FLAGS)
set(FFmpeg_HWACCEL_INCLUDE_DIRS)
set(FFmpeg_HWACCEL_LDFLAGS)

if (UNIX AND NOT ANDROID)
    find_package(PkgConfig REQUIRED)
    if (NOT ANDROID)
        pkg_check_modules(LIBVA libva)
        pkg_check_modules(CUDA cuda)
        pkg_check_modules(FFNVCODEC ffnvcodec)
        pkg_check_modules(VDPAU vdpau)
    endif()

    if (NOT APPLE)
        # In Solaris needs explicit linking for ffmpeg which links to /lib/amd64/libX11.so
        if(PLATFORM_SUN)
            find_library(LIBDRM_LIB libdrm PATHS /usr/lib/64 /usr/lib/amd64 /usr/lib)
            if(LIBDRM_LIB)
                list(APPEND FFmpeg_HWACCEL_LIBRARIES
                    X11
                    "${LIBDRM_LIB}")
                message(STATUS "Found libdrm at: ${LIBDRM_LIB}")
            else()
                message(WARNING "libdrm not found, disabling libdrm support")
                list(APPEND FFmpeg_HWACCEL_FLAGS
                    --disable-libdrm)
            endif()
        else()
            pkg_check_modules(LIBDRM libdrm REQUIRED)
            list(APPEND FFmpeg_HWACCEL_LIBRARIES
                ${LIBDRM_LIBRARIES})
            list(APPEND FFmpeg_HWACCEL_INCLUDE_DIRS
                ${LIBDRM_INCLUDE_DIRS})
            list(APPEND FFmpeg_HWACCEL_FLAGS
                --enable-libdrm)
        endif()
    endif()

    if(LIBVA_FOUND)
        find_package(X11 REQUIRED)
        pkg_check_modules(LIBVA-DRM libva-drm REQUIRED)
        pkg_check_modules(LIBVA-X11 libva-x11 REQUIRED)
        list(APPEND FFmpeg_HWACCEL_LIBRARIES
            ${X11_LIBRARIES}
            ${LIBVA-DRM_LIBRARIES}
            ${LIBVA-X11_LIBRARIES}
            ${LIBVA_LIBRARIES})
        list(APPEND FFmpeg_HWACCEL_FLAGS
            --enable-hwaccel=h264_vaapi
            --enable-hwaccel=vp8_vaapi
            --enable-hwaccel=vp9_vaapi)
        list(APPEND FFmpeg_HWACCEL_INCLUDE_DIRS
            ${X11_INCLUDE_DIRS}
            ${LIBVA-DRM_INCLUDE_DIRS}
            ${LIBVA-X11_INCLUDE_DIRS}
            ${LIBVA_INCLUDE_DIRS}
        )
        message(STATUS "ffmpeg: va-api libraries version ${LIBVA_VERSION} found")
    else()
        list(APPEND FFmpeg_HWACCEL_FLAGS --disable-vaapi)
        message(WARNING "ffmpeg: libva-dev not found, disabling Video Acceleration API (VA-API)...")
    endif()

    if (FFNVCODEC_FOUND)
        list(APPEND FFmpeg_HWACCEL_FLAGS
            --enable-cuvid
            --enable-ffnvcodec
            --enable-nvdec
            --enable-hwaccel=h264_nvdec
            --enable-hwaccel=vp8_nvdec
            --enable-hwaccel=vp9_nvdec
        )
        list(APPEND FFmpeg_HWACCEL_LIBRARIES ${FFNVCODEC_LIBRARIES})
        list(APPEND FFmpeg_HWACCEL_INCLUDE_DIRS ${FFNVCODEC_INCLUDE_DIRS})
        list(APPEND FFmpeg_HWACCEL_LDFLAGS ${FFNVCODEC_LDFLAGS})
        message(STATUS "ffmpeg: ffnvcodec libraries version ${FFNVCODEC_VERSION} found")
        # ffnvenc could load CUDA libraries at the runtime using dlopen/dlsym or LoadLibrary/GetProcAddress
        # here we handle the hard-linking scenario where CUDA is linked during compilation
        if (CUDA_FOUND)
            # This line causes build error if CUDA_INCLUDE_DIRS is anything but a single non-empty value
            #list(APPEND FFmpeg_HWACCEL_FLAGS --extra-cflags=-I${CUDA_INCLUDE_DIRS})
            list(APPEND FFmpeg_HWACCEL_LIBRARIES ${CUDA_LIBRARIES})
            list(APPEND FFmpeg_HWACCEL_INCLUDE_DIRS ${CUDA_INCLUDE_DIRS})
            list(APPEND FFmpeg_HWACCEL_LDFLAGS ${CUDA_LDFLAGS})
            message(STATUS "ffmpeg: CUDA libraries version ${CUDA_VERSION} found, hard-linking will be performed")
        endif(CUDA_FOUND)
    endif()

    if (VDPAU_FOUND AND NOT APPLE)
        list(APPEND FFmpeg_HWACCEL_FLAGS
            --enable-vdpau
            --enable-hwaccel=h264_vdpau
            --enable-hwaccel=vp9_vdpau
        )
        list(APPEND FFmpeg_HWACCEL_LIBRARIES ${VDPAU_LIBRARIES})
        list(APPEND FFmpeg_HWACCEL_INCLUDE_DIRS ${VDPAU_INCLUDE_DIRS})
        list(APPEND FFmpeg_HWACCEL_LDFLAGS ${VDPAU_LDFLAGS})
        message(STATUS "ffmpeg: vdpau libraries version ${VDPAU_VERSION} found")
    else()
        list(APPEND FFmpeg_HWACCEL_FLAGS --disable-vdpau)
        message(WARNING "ffmpeg: libvdpau-dev not found, disabling Video Decode and Presentation API for Unix (VDPAU)...")
    endif()
endif()

if (YUZU_USE_BUNDLED_FFMPEG)
    # MSVC conflicts with ksuser otherwise
    # MinGW has the funny quirk of requiring avutil after avcodec
    # Android needs some deps to be compiled with PIC (TODO)
    # TODO(crueter) fix
    if (ANDROID)
        set(BUILD_SHARED_LIBS ON)
    else()
        set(BUILD_SHARED_LIBS OFF)
    endif()

    AddJsonPackage(ffmpeg-ci)

    set(FFmpeg_INCLUDE_DIR
        "${FFmpeg_SOURCE_DIR}/include;${FFmpeg_HWACCEL_INCLUDE_DIRS}"
        PARENT_SCOPE
    )

    set(FFmpeg_PATH
        "${FFmpeg_SOURCE_DIR}"
        PARENT_SCOPE
    )

    set(FFmpeg_LIBRARY_DIR
        "${FFmpeg_SOURCE_DIR}/bin"
        PARENT_SCOPE
    )

    set(FFmpeg_LIBRARIES
        FFmpeg::FFmpeg
        ${FFmpeg_HWACCEL_LIBRARIES}
        PARENT_SCOPE
    )
else()
    # Build FFmpeg from externals
    message(STATUS "Using FFmpeg from externals")

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|amd64)")
        # FFmpeg has source that requires one of nasm or yasm to assemble it.
        # REQUIRED throws an error if not found here during configuration rather than during compilation.
        find_program(ASSEMBLER NAMES nasm yasm)
        if ("${ASSEMBLER}" STREQUAL "ASSEMBLER-NOTFOUND")
            message(FATAL_ERROR "One of either `nasm` or `yasm` not found but is required.")
        endif()
    endif()

    find_program(AUTOCONF autoconf)
    if ("${AUTOCONF}" STREQUAL "AUTOCONF-NOTFOUND")
        message(FATAL_ERROR "Required program `autoconf` not found.")
    endif()

    AddJsonPackage(ffmpeg)

    set(FFmpeg_PREFIX ${ffmpeg_SOURCE_DIR})
    set(FFmpeg_BUILD_DIR ${ffmpeg_BINARY_DIR})
    set(FFmpeg_MAKEFILE ${FFmpeg_BUILD_DIR}/Makefile)
    make_directory(${FFmpeg_BUILD_DIR})

    # Read version string from external
    file(READ ${FFmpeg_PREFIX}/RELEASE FFmpeg_VERSION)
    set(FFmpeg_FOUND NO)
    if (NOT FFmpeg_VERSION STREQUAL "")
        set(FFmpeg_FOUND YES)
    endif()

    unset(FFmpeg_LIBRARIES CACHE)

    foreach(COMPONENT ${FFmpeg_COMPONENTS})
        set(FFmpeg_${COMPONENT}_PREFIX "${FFmpeg_BUILD_DIR}/lib${COMPONENT}")
        set(FFmpeg_${COMPONENT}_LIB_NAME "lib${COMPONENT}.a")
        set(FFmpeg_${COMPONENT}_LIBRARY "${FFmpeg_${COMPONENT}_PREFIX}/${FFmpeg_${COMPONENT}_LIB_NAME}")

        set(FFmpeg_LIBRARIES
            ${FFmpeg_LIBRARIES}
            ${FFmpeg_${COMPONENT}_LIBRARY}
            CACHE PATH "Paths to FFmpeg libraries" FORCE)
    endforeach()

    find_program(BASH_PROGRAM bash REQUIRED)

    set(FFmpeg_CROSS_COMPILE_FLAGS "")
    if (ANDROID)
        string(TOLOWER "${CMAKE_HOST_SYSTEM_NAME}" FFmpeg_HOST_SYSTEM_NAME)
        set(TOOLCHAIN "${ANDROID_NDK}/toolchains/llvm/prebuilt/${FFmpeg_HOST_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR}")
        set(SYSROOT "${TOOLCHAIN}/sysroot")
        set(FFmpeg_CPU "armv8-a")
        list(APPEND FFmpeg_CROSS_COMPILE_FLAGS
            --arch=arm64
            #--cpu=${FFmpeg_CPU}
            --enable-cross-compile
            --cross-prefix=${TOOLCHAIN}/bin/aarch64-linux-android-
            --sysroot=${SYSROOT}
            --target-os=android
            --extra-ldflags="--ld-path=${TOOLCHAIN}/bin/ld.lld"
            --extra-ldflags="-nostdlib"
        )
    endif()

    # `configure` parameters builds only exactly what yuzu needs from FFmpeg
    # `--disable-vdpau` is needed to avoid linking issues
    set(FFmpeg_CC ${CMAKE_C_COMPILER_LAUNCHER}  ${CMAKE_C_COMPILER})
    set(FFmpeg_CXX ${CMAKE_CXX_COMPILER_LAUNCHER}  ${CMAKE_CXX_COMPILER})
    add_custom_command(
        OUTPUT
            ${FFmpeg_MAKEFILE}
        COMMAND
            ${BASH_PROGRAM} ${FFmpeg_PREFIX}/configure
                --disable-avdevice
                --disable-avformat
                --disable-doc
                --disable-everything
                --disable-ffmpeg
                --disable-ffprobe
                --disable-network
                --disable-postproc
                --disable-swresample
                --enable-decoder=h264
                --enable-decoder=vp8
                --enable-decoder=vp9
                --enable-filter=yadif,scale
                --enable-pic
                --cc="${FFmpeg_CC}"
                --cxx="${FFmpeg_CXX}"
                ${FFmpeg_HWACCEL_FLAGS}
                ${FFmpeg_CROSS_COMPILE_FLAGS}
        WORKING_DIRECTORY
            ${FFmpeg_BUILD_DIR}
    )
    unset(FFmpeg_CC)
    unset(FFmpeg_CXX)
    unset(FFmpeg_HWACCEL_FLAGS)
    unset(FFmpeg_CROSS_COMPILE_FLAGS)

    # Workaround for Ubuntu 18.04's older version of make not being able to call make as a child
    # with context of the jobserver. Also helps ninja users.
    execute_process(
        COMMAND
            nproc
        OUTPUT_VARIABLE
            SYSTEM_THREADS)

    set(FFmpeg_BUILD_LIBRARIES ${FFmpeg_LIBRARIES})

    # BSD make or Solaris make don't support ffmpeg make-j8
    if (PLATFORM_LINUX OR ANDROID OR APPLE OR WIN32 OR PLATFORM_FREEBSD)
        set(FFmpeg_MAKE_ARGS -j${SYSTEM_THREADS})
    else()
        set(FFmpeg_MAKE_ARGS "")
    endif()

    add_custom_command(
        OUTPUT
            ${FFmpeg_BUILD_LIBRARIES}
        COMMAND
            make ${FFmpeg_MAKE_ARGS}
        WORKING_DIRECTORY
            ${FFmpeg_BUILD_DIR}
    )

    set(FFmpeg_INCLUDE_DIR
        "${FFmpeg_PREFIX};${FFmpeg_BUILD_DIR};${FFmpeg_HWACCEL_INCLUDE_DIRS}"
        CACHE PATH "Path to FFmpeg headers" FORCE)

    set(FFmpeg_LDFLAGS
        "${FFmpeg_HWACCEL_LDFLAGS}"
        CACHE STRING "FFmpeg linker flags" FORCE)

    # ALL makes this custom target build every time
    # but it won't actually build if the DEPENDS parameter is up to date
    add_custom_target(ffmpeg-configure ALL DEPENDS ${FFmpeg_MAKEFILE})
    add_custom_target(ffmpeg-build ALL DEPENDS ${FFmpeg_BUILD_LIBRARIES} ffmpeg-configure)
    link_libraries(${FFmpeg_LIBVA_LIBRARIES})
    set(FFmpeg_LIBRARIES ${FFmpeg_BUILD_LIBRARIES} ${FFmpeg_HWACCEL_LIBRARIES}
        CACHE PATH "Paths to FFmpeg libraries" FORCE)
    unset(FFmpeg_BUILD_LIBRARIES)
    unset(FFmpeg_HWACCEL_FLAGS)
    unset(FFmpeg_HWACCEL_INCLUDE_DIRS)
    unset(FFmpeg_HWACCEL_LDFLAGS)
    unset(FFmpeg_HWACCEL_LIBRARIES)

    if (FFmpeg_FOUND)
        message(STATUS "Found FFmpeg version ${FFmpeg_VERSION}")
    else()
        message(FATAL_ERROR "FFmpeg not found")
    endif()
endif()

unset(FFmpeg_COMPONENTS)
