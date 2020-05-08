#!/bin/bash -ex

chmod a+x ./.ci/scripts/format/docker.sh
# the UID for the container yuzu user is 1027
sudo chown -R 1027 ./
docker run -v $(pwd):/yuzu yuzuemu/build-environments:linux-clang-format /bin/bash -ex /yuzu/.ci/scripts/format/docker.sh
sudo chown -R $UID ./
