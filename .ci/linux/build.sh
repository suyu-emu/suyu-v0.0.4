#!/bin/bash -ex

# SPDX-FileCopyrightText: 2025 eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

export ARCH="$(uname -m)"

if [ "$ARCH" = 'x86_64' ]; then
	if [ "$1" = 'v3' ]; then
		echo "Making x86-64-v3 optimized build of eden"
		ARCH="${ARCH}_v3"
		ARCH_FLAGS="-march=x86-64-v3 -O3"
	else
		echo "Making x86-64 generic build of eden"
		ARCH_FLAGS="-march=x86-64 -mtune=generic -O3"
	fi
else
	echo "Making aarch64 build of eden"
	ARCH_FLAGS="-march=armv8-a -mtune=generic -O3"
fi

NPROC="$2"
if [ -z "$NPROC" ]; then
    NPROC="$(nproc)"
fi

if [ "$TARGET" = "appimage" ]; then
    export EXTRA_CMAKE_FLAGS=(-DCMAKE_INSTALL_PREFIX=/usr)
    # Bundle required QT wayland libraries
    export EXTRA_QT_PLUGINS="waylandcompositor"
    export EXTRA_PLATFORM_PLUGINS="libqwayland-egl.so;libqwayland-generic.so"
else
    # For the linux-fresh verification target, verify compilation without PCH as well.
    export EXTRA_CMAKE_FLAGS=(-DYUZU_USE_PRECOMPILED_HEADERS=OFF)
fi

# TODO(crueter): update checker
# if [ "$GITHUB_REF_TYPE" == "tag" ]; then
# 	export EXTRA_CMAKE_FLAGS=($EXTRA_CMAKE_FLAGS -DENABLE_QT_UPDATE_CHECKER=ON)
# fi

mkdir -p build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
	-DENABLE_QT_TRANSLATION=ON \
    -DUSE_DISCORD_PRESENCE=ON \
    -DCMAKE_CXX_FLAGS="$ARCH_FLAGS" \
    -DCMAKE_C_FLAGS="$ARCH_FLAGS" \
    -DYUZU_USE_BUNDLED_VCPKG=OFF \
    -DYUZU_USE_BUNDLED_QT=OFF \
    -DYUZU_USE_BUNDLED_FFMPEG=OFF \
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
