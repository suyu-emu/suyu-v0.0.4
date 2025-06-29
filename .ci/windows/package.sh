GITDATE=$(git show -s --date=short --format='%ad' | tr -d "-")
GITREV=$(git show -s --format='%h')

ZIP_NAME="Eden-Windows-${ARCH}-${GITDATE}-${GITREV}.zip"

mkdir -p artifacts
mkdir -p pack

cp -r build/pkg/* pack

cp LICENSE* README* pack/

7z a -tzip artifacts/$ZIP_NAME pack/*