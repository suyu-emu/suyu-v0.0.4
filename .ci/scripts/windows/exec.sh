#!/bin/bash -ex

# SPDX-FileCopyrightText: 2019 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

mkdir -p "ccache" || true
chmod a+x ./.ci/scripts/windows/docker.sh
# the UID for the container suyu user is 1027
sudo chown -R 1027 ./
docker run -e CCACHE_DIR=/suyu/ccache -v "$(pwd):/suyu" -w /suyu suyuemu/build-environments:linux-mingw /bin/bash -ex /suyu/.ci/scripts/windows/docker.sh "$1"
sudo chown -R $UID ./
