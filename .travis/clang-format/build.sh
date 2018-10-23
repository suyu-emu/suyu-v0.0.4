#!/bin/bash -ex

docker run --env-file .travis/common/travis-ci.env -v $(pwd):/yuzu -v "$HOME/.ccache":/root/.ccache citraemu/build-environments:linux-clang-format /bin/bash -ex /yuzu/.travis/clang-format/docker.sh
