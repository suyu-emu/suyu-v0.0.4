#!/bin/sh

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

GITDATE="$(git show -s --date=short --format='%ad' | sed 's/-//g')"
GITREV="$(git show -s --format='%h')"
ARTIFACTS_DIR="$PWD/artifacts"
mkdir -p "${ARTIFACTS_DIR}/"

REV_NAME="eden-android-${GITDATE}-${GITREV}"
BUILD_FLAVOR="mainline"
BUILD_TYPE_LOWER="release"
BUILD_TYPE_UPPER="Release"

cp src/android/app/build/outputs/apk/"${BUILD_FLAVOR}/${BUILD_TYPE_LOWER}/app-${BUILD_FLAVOR}-${BUILD_TYPE_LOWER}.apk" \
	"${ARTIFACTS_DIR}/${REV_NAME}.apk" || echo "APK not found"

cp src/android/app/build/outputs/bundle/"${BUILD_FLAVOR}${BUILD_TYPE_UPPER}"/"app-${BUILD_FLAVOR}-${BUILD_TYPE_LOWER}.aab" \
	"${ARTIFACTS_DIR}/${REV_NAME}.aab" || echo "AAB not found"

ls -la "${ARTIFACTS_DIR}/"
