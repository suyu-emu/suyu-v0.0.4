#!/bin/bash -ex

# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

. ./.ci/scripts/common/pre-upload.sh

REV_NAME="yuzu-${GITDATE}-${GITREV}"

BUILD_FLAVOR=mainline

cp src/android/app/build/outputs/apk/"${BUILD_FLAVOR}/release/app-${BUILD_FLAVOR}-release.apk" \
  "artifacts/${REV_NAME}.apk"
cp src/android/app/build/outputs/bundle/"${BUILD_FLAVOR}Release"/"app-${BUILD_FLAVOR}-release.aab" \
  "artifacts/${REV_NAME}.aab"
