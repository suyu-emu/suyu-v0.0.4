#!/bin/bash -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

case "$1" in
amd64 | "")
    echo "Making amd64-v3 optimized build of Eden"
    ARCH="amd64_v3"
    ARCH_FLAGS="-march=x86-64-v3"
    export EXTRA_CMAKE_FLAGS=(-DYUZU_BUILD_PRESET=v3)
    ;;
steamdeck | zen2)
    echo "Making Steam Deck (Zen 2) optimized build of Eden"
    ARCH="steamdeck"
    ARCH_FLAGS="-march=znver2 -mtune=znver2"
    export EXTRA_CMAKE_FLAGS=(-DYUZU_BUILD_PRESET=zen2 -DYUZU_SYSTEM_PROFILE=steamdeck)
    ;;
rog-ally | allyx | zen4)
    echo "Making ROG Ally X (Zen 4) optimized build of Eden"
    ARCH="rog-ally-x"
    ARCH_FLAGS="-march=znver4 -mtune=znver4"
    export EXTRA_CMAKE_FLAGS=(-DYUZU_BUILD_PRESET=zen2 -DYUZU_SYSTEM_PROFILE=steamdeck)
    ;;
legacy)
    echo "Making amd64 generic build of Eden"
    ARCH=amd64
    ARCH_FLAGS="-march=x86-64 -mtune=generic"
    export EXTRA_CMAKE_FLAGS=(-DYUZU_BUILD_PRESET=generic)
    ;;
aarch64)
    echo "Making armv8-a build of Eden"
    ARCH=aarch64
    ARCH_FLAGS="-march=armv8-a -mtune=generic -w"
    export EXTRA_CMAKE_FLAGS=(-DYUZU_BUILD_PRESET=generic)
    ;;
armv9)
    echo "Making armv9-a build of Eden"
    ARCH=armv9
    ARCH_FLAGS="-march=armv9-a -mtune=generic -w"
    export EXTRA_CMAKE_FLAGS=(-DYUZU_BUILD_PRESET=armv9)
    ;;
native)
    echo "Making native build of Eden"
    ARCH="$(uname -m)"
    ARCH_FLAGS="-march=native -mtune=native"
    export EXTRA_CMAKE_FLAGS=(-DYUZU_BUILD_PRESET=native)
    ;;
*)
    echo "Invalid target $1 specified, must be one of native, amd64, steamdeck, zen2, allyx, rog-ally, zen4, legacy, aarch64, armv9"
    exit 1
    ;;
esac

export ARCH_FLAGS="$ARCH_FLAGS -O3"

if [ -z "$NPROC" ]; then
    NPROC="$(nproc)"
fi

if [ "$1" != "" ]; then shift; fi

if [ "$TARGET" = "appimage" ]; then
    export EXTRA_CMAKE_FLAGS=("${EXTRA_CMAKE_FLAGS[@]}" -DCMAKE_INSTALL_PREFIX=/usr -DYUZU_ROOM=ON -DYUZU_ROOM_STANDALONE=OFF -DYUZU_CMD=OFF)
else
    # For the linux-fresh verification target, verify compilation without PCH as well.
    export EXTRA_CMAKE_FLAGS=("${EXTRA_CMAKE_FLAGS[@]}" -DYUZU_USE_PRECOMPILED_HEADERS=OFF)
fi


if [ "$DEVEL" != "true" ]; then
    export EXTRA_CMAKE_FLAGS=("${EXTRA_CMAKE_FLAGS[@]}" -DENABLE_UPDATE_CHECKER=ON)
fi

if [ "$USE_WEBENGINE" = "true" ]; then
    WEBENGINE=ON
else
    WEBENGINE=OFF
fi

if [ "$USE_MULTIMEDIA" = "false" ]; then
    MULTIMEDIA=OFF
else
    MULTIMEDIA=ON
fi

if [ -z "$BUILD_TYPE" ]; then
    export BUILD_TYPE="Release"
fi

export EXTRA_CMAKE_FLAGS=("${EXTRA_CMAKE_FLAGS[@]}" $@)

mkdir -p build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DENABLE_QT_TRANSLATION=ON \
    -DUSE_DISCORD_PRESENCE=ON \
    -DCMAKE_CXX_FLAGS="$ARCH_FLAGS" \
    -DCMAKE_C_FLAGS="$ARCH_FLAGS" \
    -DYUZU_USE_BUNDLED_QT=OFF \
    -DYUZU_USE_BUNDLED_SDL2=OFF \
    -DYUZU_USE_EXTERNAL_SDL2=ON \
    -DYUZU_TESTS=OFF \
    -DYUZU_USE_QT_MULTIMEDIA=$MULTIMEDIA \
    -DYUZU_USE_QT_WEB_ENGINE=$WEBENGINE \
    -DYUZU_USE_FASTER_LD=ON \
    -DYUZU_ENABLE_LTO=ON \
    -DDYNARMIC_ENABLE_LTO=ON \
    "${EXTRA_CMAKE_FLAGS[@]}"

ninja -j${NPROC}

if [ -d "bin/Release" ]; then
    strip -s bin/Release/*
else
    strip -s bin/*
fi
