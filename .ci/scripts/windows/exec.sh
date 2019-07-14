#!/bin/bash -ex

mkdir -p "ccache" || true
chmod a+x ./.ci/scripts/windows/docker.sh
docker run -e CCACHE_DIR=/yuzu/ccache -v $(pwd):/yuzu yuzuemu/build-environments:linux-mingw /bin/bash -ex /yuzu/.ci/scripts/windows/docker.sh
