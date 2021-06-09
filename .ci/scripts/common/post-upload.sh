#!/bin/bash -ex

# Copy documentation
cp license.txt "$DIR_NAME"
cp README.md "$DIR_NAME"

tar -cJvf "${REV_NAME}-source.tar.xz" src externals CMakeLists.txt README.md license.txt
cp "${REV_NAME}-source.tar.xz" "$DIR_NAME"

tar $COMPRESSION_FLAGS "$ARCHIVE_NAME" "$DIR_NAME"

# move the compiled archive into the artifacts directory to be uploaded by travis releases
mv "$ARCHIVE_NAME" "${ARTIFACTS_DIR}/"
