# SPDX-FileCopyrightText: 2025 eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

$target=$args[0]
$debug=$args[1]

$GITDATE = $(git show -s --date=short --format='%ad') -replace "-", ""
$GITREV = $(git show -s --format='%h')
$RELEASE_DIST = "eden-windows-msvc"
$ARTIFACTS_DIR = "artifacts"

New-Item -ItemType Directory -Path $ARTIFACTS_DIR -Force
New-Item -ItemType Directory -Path $RELEASE_DIST -Force

if ($debug -eq "yes") {
	mkdir -p pdb
	$BUILD_PDB = "eden-windows-msvc-$GITDATE-$GITREV-debugsymbols.zip"
	Get-ChildItem "build/$target/bin/" -Recurse -Filter "*.pdb" | Copy-Item -destination .\pdb -ErrorAction SilentlyContinue

	if (Test-Path -Path ".\pdb\*.pdb") {
		7z a -tzip $BUILD_PDB .\pdb\*.pdb
		Move-Item $BUILD_PDB $ARTIFACTS_DIR/ -ErrorAction SilentlyContinue
	}
} else {
	Get-ChildItem "build/$target/bin/" -Recurse -Filter "*.pdb" | Remove-Item -Force -ErrorAction SilentlyContinue
}

Copy-Item "build/$target/bin/Release/*" -Destination "$RELEASE_DIST" -Recurse -ErrorAction SilentlyContinue
if (-not $?) {
	# Try without Release subfolder if that doesn't exist
	Copy-Item "build/$target/bin/*" -Destination "$RELEASE_DIST" -Recurse -ErrorAction SilentlyContinue
}


$BUILD_ZIP = "eden-windows-msvc-$GITDATE-$GITREV.zip"

7z a -tzip $BUILD_ZIP $RELEASE_DIST\*

Move-Item $BUILD_ZIP $ARTIFACTS_DIR/ -Force #-ErrorAction SilentlyContinue
Copy-Item "LICENSE*" -Destination "$RELEASE_DIST" -ErrorAction SilentlyContinue
Copy-Item "README*" -Destination "$RELEASE_DIST" -ErrorAction SilentlyContinue
