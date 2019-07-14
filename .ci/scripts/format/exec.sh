#!/bin/bash -ex

chmod a+x ./.ci/scripts/format/docker.sh
docker run -v $(pwd):/yuzu yuzuemu/build-environments:linux-clang-format /bin/bash -ex /yuzu/.ci/scripts/format/docker.sh
