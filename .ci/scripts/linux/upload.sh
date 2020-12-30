#!/bin/bash -ex

. .ci/scripts/common/pre-upload.sh

APPIMAGE_NAME="yuzu-x86_64.AppImage"
NEW_APPIMAGE_NAME="yuzu-${GITDATE}-${GITREV}-x86_64.AppImage"
REV_NAME="yuzu-linux-${GITDATE}-${GITREV}"
ARCHIVE_NAME="${REV_NAME}.tar.xz"
COMPRESSION_FLAGS="-cJvf"

if [ "${RELEASE_NAME}" = "mainline" ]; then
    DIR_NAME="${REV_NAME}"
else
    DIR_NAME="${REV_NAME}_${RELEASE_NAME}"
fi

mkdir "$DIR_NAME"

cp build/bin/yuzu-cmd "$DIR_NAME"
cp build/bin/yuzu "$DIR_NAME"

# Copy the AppImage to the artifacts directory and avoid compressing it
cp "build/${APPIMAGE_NAME}" "${ARTIFACTS_DIR}/${NEW_APPIMAGE_NAME}"

. .ci/scripts/common/post-upload.sh
