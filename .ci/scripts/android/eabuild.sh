#!/bin/bash -ex

# SPDX-FileCopyrightText: 2024 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

export NDK_CCACHE="$(which ccache)"
ccache -s

export ANDROID_KEYSTORE_FILE="${GITHUB_WORKSPACE}/ks.jks"
base64 --decode <<< "${EA_PLAY_ANDROID_KEYSTORE_B64}" > "${ANDROID_KEYSTORE_FILE}"
export ANDROID_KEY_ALIAS="${PLAY_ANDROID_KEY_ALIAS}"
export ANDROID_KEYSTORE_PASS="${PLAY_ANDROID_KEYSTORE_PASS}"
export SERVICE_ACCOUNT_KEY_PATH="${GITHUB_WORKSPACE}/sa.json"
base64 --decode <<< "${EA_SERVICE_ACCOUNT_KEY_B64}" > "${SERVICE_ACCOUNT_KEY_PATH}"
./gradlew "publishEaReleaseBundle"

ccache -s

if [ ! -z "${ANDROID_KEYSTORE_B64}" ]; then
    rm "${ANDROID_KEYSTORE_FILE}"
fi
