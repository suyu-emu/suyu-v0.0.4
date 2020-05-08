#!/bin/bash -ex

mkdir -p "ccache" || true
chmod a+x ./.ci/scripts/windows/docker.sh
# the UID for the container yuzu user is 1027
sudo chown -R 1027 ./
docker run -e CCACHE_DIR=/yuzu/ccache -v $(pwd):/yuzu yuzuemu/build-environments:linux-mingw /bin/bash -ex /yuzu/.ci/scripts/windows/docker.sh $1
sudo chown -R $UID ./
