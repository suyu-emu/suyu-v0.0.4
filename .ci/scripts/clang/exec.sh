#!/bin/bash -ex

# SPDX-FileCopyrightText: 2024 suyu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
# 
# Modified by AMA25 on 3/5/24

mkdir -p "ccache"  || true
chmod a+x ./.ci/scripts/clang/docker.sh
# the UID for the container suyu user is 1027
sudo chown -R 1027 ./
docker run -e ENABLE_COMPATIBILITY_REPORTING -e CCACHE_DIR=/suyu/ccache -v "$(pwd):/suyu" -w /suyu suyuemu/build-environments:linux-fresh /bin/bash /suyu/.ci/scripts/clang/docker.sh "$1"
sudo chown -R $UID ./
