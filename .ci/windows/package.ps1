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

Copy-Item -Path "build/$target/bin/*" -Destination "$RELEASE_DIST" -Recurse -ErrorAction SilentlyContinue -Force

if ($debug -eq "yes") {
	mkdir -p pdb
	Get-ChildItem -Path "$RELEASE_DIST" -Filter "*.pdb" -Recurse | Move-Item -Destination .\pdb -ErrorAction SilentlyContinue -Force
	
	if (Test-Path -Path ".\pdb\*.pdb") {
		$BUILD_PDB = "eden-windows-msvc-$GITDATE-$GITREV-debugsymbols.zip"
		7z a -tzip $BUILD_PDB .\pdb\*.pdb
		Move-Item -Path $BUILD_PDB -Destination $ARTIFACTS_DIR/ -ErrorAction SilentlyContinue -Force
	}
}

if ($debug -ne "yes") {
	Remove-Item "$RELEASE_DIST\*.pdb" -Recurse -ErrorAction SilentlyContinue -Force
}

Move-Item -Path "$RELEASE_DIST\Release\*" -Destination "$RELEASE_DIST" -ErrorAction SilentlyContinue -Force
Remove-Item "$RELEASE_DIST\Release" -ErrorAction SilentlyContinue -Force

$BUILD_ZIP = "eden-windows-msvc-$GITDATE-$GITREV.zip"
7z a -tzip $BUILD_ZIP $RELEASE_DIST\*
Move-Item -Path $BUILD_ZIP -Destination $ARTIFACTS_DIR/ -ErrorAction SilentlyContinue -Force

Copy-Item "LICENSE*" -Destination "$RELEASE_DIST" -ErrorAction SilentlyContinue
Copy-Item "README*" -Destination "$RELEASE_DIST" -ErrorAction SilentlyContinue
