#!/bin/bash -ex

mkdir -p "$HOME/.ccache"
docker run -e ENABLE_COMPATIBILITY_REPORTING --env-file .travis/common/travis-ci.env -v $(pwd):/yuzu -v "$HOME/.ccache":/root/.ccache ubuntu:18.04 /bin/bash /yuzu/.travis/linux/docker.sh
