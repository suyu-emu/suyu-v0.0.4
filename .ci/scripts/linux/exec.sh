#!/bin/bash -ex

mkdir -p "ccache"  || true
chmod a+x ./.ci/scripts/linux/docker.sh
docker run -e ENABLE_COMPATIBILITY_REPORTING -e CCACHE_DIR=/yuzu/ccache -v $(pwd):/yuzu yuzuemu/build-environments:linux-fresh /bin/bash /yuzu/.ci/scripts/linux/docker.sh $1
