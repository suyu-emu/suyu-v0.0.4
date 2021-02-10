#!/bin/bash -ex

# Exit on error, rather than continuing with the rest of the script.
set -e

cd /yuzu

ccache -s

mkdir build || true && cd build
cmake .. -DDISPLAY_VERSION=$1 -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/lib/ccache/clang -DCMAKE_CXX_COMPILER=/usr/lib/ccache/clang++ -DYUZU_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"} -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON -DUSE_DISCORD_PRESENCE=ON -DENABLE_QT_TRANSLATION=ON -DCMAKE_INSTALL_PREFIX="/usr"

make -j$(nproc)

ccache -s

ctest -VV -C Release

