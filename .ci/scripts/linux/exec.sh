#!/bin/bash -ex

# SPDX-FileCopyrightText: 2019 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

mkdir -p "ccache"  || true
chmod a+x ./.ci/scripts/linux/docker.sh
# the UID for the container suyu user is 1027
sudo chown -R 1027 ./

# The environment variables listed below:
# AZURECIREPO TITLEBARFORMATIDLE TITLEBARFORMATRUNNING DISPLAYVERSION
#  are requested in src/common/CMakeLists.txt and appear to be provided somewhere in Azure DevOps

docker run -e AZURECIREPO -e TITLEBARFORMATIDLE -e TITLEBARFORMATRUNNING -e DISPLAYVERSION -e ENABLE_COMPATIBILITY_REPORTING -e CCACHE_DIR=/suyu/ccache -v "$(pwd):/suyu" -w /suyu suyuemu/build-environments:linux-fresh /bin/bash /suyu/.ci/scripts/linux/docker.sh "$1"
sudo chown -R $UID ./
