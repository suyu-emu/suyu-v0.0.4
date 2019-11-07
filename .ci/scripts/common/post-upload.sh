#!/bin/bash -ex

# Copy documentation
cp license.txt "$DIR_NAME"
cp README.md "$DIR_NAME"

tar $COMPRESSION_FLAGS "$ARCHIVE_NAME" "$DIR_NAME"

mv "$DIR_NAME" $RELEASE_NAME

7z a "$REV_NAME.7z" $RELEASE_NAME

# move the compiled archive into the artifacts directory to be uploaded by travis releases
mv "$ARCHIVE_NAME" artifacts/
mv "$REV_NAME.7z" artifacts/
