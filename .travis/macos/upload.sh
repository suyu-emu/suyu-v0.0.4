#!/bin/bash -ex

. .travis/common/pre-upload.sh

REV_NAME="yuzu-osx-${GITDATE}-${GITREV}"
ARCHIVE_NAME="${REV_NAME}.tar.gz"
COMPRESSION_FLAGS="-czvf"

mkdir "$REV_NAME"

cp build/bin/yuzu-cmd "$REV_NAME"
cp -r build/bin/yuzu.app "$REV_NAME"

# move libs into folder for deployment
macpack "${REV_NAME}/yuzu.app/Contents/MacOS/yuzu" -d "../Frameworks"
# move qt frameworks into app bundle for deployment
$(brew --prefix)/opt/qt5/bin/macdeployqt "${REV_NAME}/yuzu.app" -executable="${REV_NAME}/yuzu.app/Contents/MacOS/yuzu"

# move libs into folder for deployment
macpack "${REV_NAME}/yuzu-cmd" -d "libs"

# Make the launching script executable
chmod +x ${REV_NAME}/yuzu.app/Contents/MacOS/yuzu

# Verify loader instructions
find "$REV_NAME" -exec otool -L {} \;

. .travis/common/post-upload.sh
