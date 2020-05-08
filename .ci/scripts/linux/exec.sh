#!/bin/bash -ex

mkdir -p "ccache"  || true
chmod a+x ./.ci/scripts/linux/docker.sh
# the UID for the container yuzu user is 1027
sudo chown -R 1027 ./
docker run -e ENABLE_COMPATIBILITY_REPORTING -e CCACHE_DIR=/yuzu/ccache -v $(pwd):/yuzu yuzuemu/build-environments:linux-fresh /bin/bash /yuzu/.ci/scripts/linux/docker.sh $1
sudo chown -R $UID ./
