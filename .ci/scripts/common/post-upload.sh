#!/bin/bash -ex

# Copy documentation
cp license.txt "$DIR_NAME"
cp README.md "$DIR_NAME"

if [[ "x${NO_SOURCE_PACK}" == "x" ]]; then
  tar -cJvf "${REV_NAME}-source.tar.xz" src externals CMakeLists.txt README.md license.txt
  cp -v "${REV_NAME}-source.tar.xz" "$DIR_NAME"
fi

tar $COMPRESSION_FLAGS "$ARCHIVE_NAME" "$DIR_NAME"

# move the compiled archive into the artifacts directory to be uploaded by travis releases
mv "$ARCHIVE_NAME" "${ARTIFACTS_DIR}/"
