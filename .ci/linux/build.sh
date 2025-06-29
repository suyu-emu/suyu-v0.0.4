#!/bin/bash -e

# SPDX-FileCopyrightText: 2025 eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

export ARCH="$(uname -m)"

case "$1" in
    amd64|"")
        echo "Making amd64-v3 optimized build of Eden"
        ARCH="amd64_v3"
        ARCH_FLAGS="-march=x86-64-v3"
        ;;
    steamdeck)
        echo "Making Steam Deck (Zen 2) optimized build of Eden"
        ARCH="steamdeck"
        ARCH_FLAGS="-march=znver2 -mtune=znver2"
        ;;
    rog-ally|allyx)
        echo "Making ROG Ally X (Zen 4) optimized build of Eden"
        ARCH="rog-ally-x"
        ARCH_FLAGS="-march=znver3 -mtune=znver4" # GH actions runner is a Zen 3 CPU, so a small workaround
        ;;
    legacy)
        echo "Making amd64 generic build of Eden"
        ARCH=amd64
        ARCH_FLAGS="-march=x86-64 -mtune=generic"
        ;;
    aarch64)
        echo "Making armv8-a build of Eden"
        ARCH=aarch64
        ARCH_FLAGS="-march=armv8-a -mtune=generic -w"
        ;;
    armv9)
        echo "Making armv9-a build of Eden"
        ARCH=armv9
        ARCH_FLAGS="-march=armv9-a -mtune=generic -w"
        ;;
esac

export ARCH_FLAGS="$ARCH_FLAGS -O3"

NPROC="$2"
if [ -z "$NPROC" ]; then
    NPROC="$(nproc)"
else
    shift
fi

if [ "$1" != "" ]; then shift; fi

if [ "$TARGET" = "appimage" ]; then
    export EXTRA_CMAKE_FLAGS=(-DCMAKE_INSTALL_PREFIX=/usr -DYUZU_ROOM=ON -DYUZU_ROOM_STANDALONE=OFF -DYUZU_CMD=OFF)
else
    # For the linux-fresh verification target, verify compilation without PCH as well.
    export EXTRA_CMAKE_FLAGS=(-DYUZU_USE_PRECOMPILED_HEADERS=OFF)
fi

if [ "$DEVEL" != "true" ]; then
    export EXTRA_CMAKE_FLAGS=("${EXTRA_CMAKE_FLAGS[@]}" -DENABLE_QT_UPDATE_CHECKER=ON)
fi

export EXTRA_CMAKE_FLAGS=("${EXTRA_CMAKE_FLAGS[@]}" $@)

mkdir -p build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
	-DENABLE_QT_TRANSLATION=ON \
    -DUSE_DISCORD_PRESENCE=ON \
    -DCMAKE_CXX_FLAGS="$ARCH_FLAGS" \
    -DCMAKE_C_FLAGS="$ARCH_FLAGS" \
    -DYUZU_USE_BUNDLED_VCPKG=OFF \
    -DYUZU_USE_BUNDLED_QT=OFF \
    -DYUZU_USE_BUNDLED_SDL2=OFF \
    -DYUZU_USE_EXTERNAL_SDL2=ON \
    -DYUZU_TESTS=OFF \
    -DYUZU_USE_QT_MULTIMEDIA=ON \
    -DYUZU_USE_QT_WEB_ENGINE=ON \
    -DYUZU_USE_FASTER_LD=ON \
    -DYUZU_ENABLE_LTO=ON \
	"${EXTRA_CMAKE_FLAGS[@]}"

ninja -j${NPROC}

if [ -d "bin/Release" ]; then
  strip -s bin/Release/*
else
  strip -s bin/*
fi
