#!/bin/bash -ex

. .ci/scripts/common/pre-upload.sh

REV_NAME="yuzu-linux-${GITDATE}-${GITREV}"
ARCHIVE_NAME="${REV_NAME}.tar.xz"
COMPRESSION_FLAGS="-cJvf"

mkdir "$REV_NAME"

cp build/bin/yuzu-cmd "$REV_NAME"
cp build/bin/yuzu "$REV_NAME"

. .ci/scripts/common/post-upload.sh
