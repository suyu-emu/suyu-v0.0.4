GITDATE=$(git show -s --date=short --format='%ad' | tr -d "-")
GITREV=$(git show -s --format='%h')

ZIP_NAME="Eden-Windows-${ARCH}-${GITDATE}-${GITREV}.zip"

ARTIFACTS_DIR="artifacts"
PKG_DIR="build/pkg"

mkdir -p "$ARTIFACTS_DIR"

TMP_DIR=$(mktemp -d)

cp -r "$PKG_DIR"/* "$TMP_DIR"/
cp LICENSE* README* "$TMP_DIR"/

7z a -tzip "$ARTIFACTS_DIR/$ZIP_NAME" "$TMP_DIR"/*

rm -rf "$TMP_DIR"