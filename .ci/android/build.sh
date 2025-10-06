#!/bin/bash -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

export NDK_CCACHE=$(which ccache)

if [ ! -z "${ANDROID_KEYSTORE_B64}" ]; then
    export ANDROID_KEYSTORE_FILE="${GITHUB_WORKSPACE}/ks.jks"
    base64 --decode <<< "${ANDROID_KEYSTORE_B64}" > "${ANDROID_KEYSTORE_FILE}"
fi

cd src/android
chmod +x ./gradlew

./gradlew assembleMainlineRelease
./gradlew bundleMainlineRelease

if [ ! -z "${ANDROID_KEYSTORE_B64}" ]; then
    rm "${ANDROID_KEYSTORE_FILE}"
fi
