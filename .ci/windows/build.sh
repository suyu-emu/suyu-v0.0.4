#!/bin/bash -e

# SPDX-FileCopyrightText: 2025 eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

if [ "$DEVEL" != "true" ]; then
    export EXTRA_CMAKE_FLAGS=("${EXTRA_CMAKE_FLAGS[@]}" -DENABLE_QT_UPDATE_CHECKER=ON)
fi

if [ "$CCACHE" = "true" ]; then
    export EXTRA_CMAKE_FLAGS=("${EXTRA_CMAKE_FLAGS[@]}" -DUSE_CCACHE=ON)
fi

if [ "$BUNDLE_QT" = "true" ]; then
    export EXTRA_CMAKE_FLAGS=("${EXTRA_CMAKE_FLAGS[@]}" -DYUZU_USE_BUNDLED_QT=ON)
else
    export EXTRA_CMAKE_FLAGS=("${EXTRA_CMAKE_FLAGS[@]}" -DYUZU_USE_BUNDLED_QT=OFF)
fi

if [ -z "$BUILD_TYPE" ]; then
    export BUILD_TYPE="Release"
fi

if [ "$WINDEPLOYQT" == "" ]; then
    echo "You must supply the WINDEPLOYQT environment variable."
    exit 1
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

export EXTRA_CMAKE_FLAGS=("${EXTRA_CMAKE_FLAGS[@]}" $@)

mkdir -p build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
	-DENABLE_QT_TRANSLATION=ON \
    -DUSE_DISCORD_PRESENCE=ON \
    -DYUZU_USE_BUNDLED_SDL2=OFF \
    -DYUZU_USE_EXTERNAL_SDL2=ON \
    -DYUZU_TESTS=OFF \
    -DYUZU_CMD=OFF \
    -DYUZU_ROOM_STANDALONE=OFF \
    -DYUZU_USE_QT_MULTIMEDIA=$MULTIMEDIA \
    -DYUZU_USE_QT_WEB_ENGINE=$WEBENGINE \
    -DYUZU_ENABLE_LTO=ON \
	  "${EXTRA_CMAKE_FLAGS[@]}"

ninja

set +e
rm -f bin/*.pdb
set -e

$WINDEPLOYQT --release --no-compiler-runtime --no-opengl-sw --no-system-dxc-compiler --no-system-d3d-compiler --dir pkg bin/eden.exe
cp bin/* pkg
