#!/bin/bash -ex

# SPDX-FileCopyrightText: 2024 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Ensure ccache is set up correctly
export NDK_CCACHE="$(which ccache)"
ccache -s

# Decode Android keystore and service account keys
export ANDROID_KEYSTORE_FILE="${GITHUB_WORKSPACE}/ks.jks"
if [ -z "${MAINLINE_PLAY_ANDROID_KEYSTORE_B64}" ]; then
    echo "Error: MAINLINE_PLAY_ANDROID_KEYSTORE_B64 is not set"
    exit 1
fi
base64 --decode <<< "${MAINLINE_PLAY_ANDROID_KEYSTORE_B64}" > "${ANDROID_KEYSTORE_FILE}"

export ANDROID_KEY_ALIAS="${PLAY_ANDROID_KEY_ALIAS}"
export ANDROID_KEYSTORE_PASS="${PLAY_ANDROID_KEYSTORE_PASS}"
export SERVICE_ACCOUNT_KEY_PATH="${GITHUB_WORKSPACE}/sa.json"
base64 --decode <<< "${MAINLINE_SERVICE_ACCOUNT_KEY_B64}" > "${SERVICE_ACCOUNT_KEY_PATH}"

# Build the mainline release bundle
./gradlew "publishMainlineReleaseBundle"

ccache -s

# Clean up sensitive files
if [ ! -z "${MAINLINE_PLAY_ANDROID_KEYSTORE_B64}" ]; then
    rm "${ANDROID_KEYSTORE_FILE}"
fi
