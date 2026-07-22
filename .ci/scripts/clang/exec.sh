#!/bin/bash -ex

# SPDX-FileCopyrightText: 2021 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Create ccache directory and ensure correct permissions
mkdir -p "ccache" || true
chmod a+x ./.ci/scripts/clang/docker.sh

# Run the Clang build inside a Docker container
sudo chown -R 1027 ./
docker run -e ENABLE_COMPATIBILITY_REPORTING \
    -e CCACHE_DIR=/suyu/ccache \
    -v "$(pwd):/suyu" \
    -w /suyu \
    suyuemu/build-environments:linux-fresh \
    /bin/bash /suyu/.ci/scripts/clang/docker.sh "$1"
sudo chown -R $UID ./
