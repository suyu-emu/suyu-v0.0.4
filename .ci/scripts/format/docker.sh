#!/bin/bash -ex

# Run clang-format
cd /yuzu
chmod a+x ./.ci/scripts/format/script.sh
./.ci/scripts/format/script.sh
