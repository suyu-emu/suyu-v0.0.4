#!/bin/bash -ex

mkdir -p "$HOME/.ccache"
docker run -e ENABLE_COMPATIBILITY_REPORTING --env-file .travis/common/travis-ci.env -v $(pwd):/yuzu -v "$HOME/.ccache":/root/.ccache yuzuemu/build-environments:linux-mingw /bin/bash /yuzu/.travis/linux-mingw/docker.sh
