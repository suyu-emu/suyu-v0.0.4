#!/bin/bash -ex

# Exit on error, rather than continuing with the rest of the script.
set -e

ccache -s

mkdir build || true && cd build
cmake .. \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=/usr/lib/ccache/clang++ \
      -DCMAKE_C_COMPILER=/usr/lib/ccache/clang \
      -DCMAKE_INSTALL_PREFIX="/usr" \
      -DCMAKE_TOOLCHAIN_FILE=${VCPKG_TOOLCHAIN_FILE} \
      -DDISPLAY_VERSION=$1 \
      -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON \
      -DENABLE_QT_TRANSLATION=ON \
      -DUSE_DISCORD_PRESENCE=ON \
      -DYUZU_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"} \
      -GNinja

ninja

ccache -s

ctest -VV -C Release

