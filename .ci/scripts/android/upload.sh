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

if [ -n "${ANDROID_KEYSTORE_B64}" ]
then
  echo "Signing apk..."
  base64 --decode <<< "${ANDROID_KEYSTORE_B64}" > ks.jks

  apksigner sign --ks ks.jks \
    --ks-key-alias "${ANDROID_KEY_ALIAS}" \
    --ks-pass env:ANDROID_KEYSTORE_PASS "artifacts/${REV_NAME}.apk"
else
  echo "No keystore specified, not signing the APK files."
fi
