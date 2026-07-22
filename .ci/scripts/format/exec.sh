#!/bin/bash -ex

# SPDX-FileCopyrightText: 2019 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

chmod a+x ./.ci/scripts/format/docker.sh
# the UID for the container suyu user is 1027
sudo chown -R 1027 ./
docker run -v "$(pwd):/suyu" -w /suyu suyuemu/build-environments:linux-clang-format /bin/bash -ex /suyu/.ci/scripts/format/docker.sh
sudo chown -R $UID ./
