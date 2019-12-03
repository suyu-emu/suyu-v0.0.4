param($BUILD_NAME)

$GITDATE = $(git show -s --date=short --format='%ad') -replace "-", ""
$GITREV = $(git show -s --format='%h')

if ("$BUILD_NAME" -eq "mainline") {
    $RELEASE_DIST = "yuzu-windows-msvc"
}
else {
    $RELEASE_DIST = "yuzu-windows-msvc-$BUILD_NAME"
}

$MSVC_BUILD_ZIP = "yuzu-windows-msvc-$GITDATE-$GITREV.zip" -replace " ", ""
$MSVC_BUILD_PDB = "yuzu-windows-msvc-$GITDATE-$GITREV-debugsymbols.zip" -replace " ", ""
$MSVC_SEVENZIP = "yuzu-windows-msvc-$GITDATE-$GITREV.7z" -replace " ", ""
$MSVC_TAR = "yuzu-windows-msvc-$GITDATE-$GITREV.tar" -replace " ", ""
$MSVC_TARXZ = "yuzu-windows-msvc-$GITDATE-$GITREV.tar.xz" -replace " ", ""
$MSVC_SOURCE = "yuzu-windows-msvc-source-$GITDATE-$GITREV" -replace " ", ""
$MSVC_SOURCE_TAR = "$MSVC_SOURCE.tar"
$MSVC_SOURCE_TARXZ = "$MSVC_SOURCE_TAR.xz"

$env:BUILD_ZIP = $MSVC_BUILD_ZIP
$env:BUILD_SYMBOLS = $MSVC_BUILD_PDB
$env:BUILD_UPDATE = $MSVC_SEVENZIP

$BUILD_DIR = ".\build\bin\Release"

# Upload debugging symbols
mkdir pdb
Get-ChildItem "$BUILD_DIR\" -Recurse -Filter "*.pdb" | Copy-Item -destination .\pdb
7z a -tzip $MSVC_BUILD_PDB .\pdb\*.pdb
rm "$BUILD_DIR\*.pdb"

# Create artifact directories
mkdir $RELEASE_DIST
mkdir $MSVC_SOURCE
mkdir "artifacts"

# Build a tar.xz for the source of the release
Copy-Item .\license.txt -Destination $MSVC_SOURCE
Copy-Item .\README.md -Destination $MSVC_SOURCE
Copy-Item .\CMakeLists.txt -Destination $MSVC_SOURCE
Copy-Item .\src -Recurse -Destination $MSVC_SOURCE
Copy-Item .\externals -Recurse -Destination $MSVC_SOURCE
Copy-Item .\dist -Recurse -Destination $MSVC_SOURCE
Copy-Item .\CMakeModules -Recurse -Destination $MSVC_SOURCE
7z a -r -ttar $MSVC_SOURCE_TAR $MSVC_SOURCE
7z a -r -txz $MSVC_SOURCE_TARXZ $MSVC_SOURCE_TAR

# Build the final release artifacts
Copy-Item $MSVC_SOURCE_TARXZ -Destination $RELEASE_DIST
Copy-Item "$BUILD_DIR\*" -Destination $RELEASE_DIST -Recurse
rm "$RELEASE_DIST\*.exe"
Get-ChildItem "$BUILD_DIR" -Recurse -Filter "yuzu*.exe" | Copy-Item -destination $RELEASE_DIST
Get-ChildItem "$BUILD_DIR" -Recurse -Filter "QtWebEngineProcess*.exe" | Copy-Item -destination $RELEASE_DIST
7z a -tzip $MSVC_BUILD_ZIP $RELEASE_DIST\*
7z a $MSVC_SEVENZIP $RELEASE_DIST

7z a -r -ttar $MSVC_TAR $RELEASE_DIST
7z a -r -txz $MSVC_TARXZ $MSVC_TAR

Get-ChildItem . -Filter "*.zip" | Copy-Item -destination "artifacts"
Get-ChildItem . -Filter "*.7z" | Copy-Item -destination "artifacts"
Get-ChildItem . -Filter "*.tar.xz" | Copy-Item -destination "artifacts"
