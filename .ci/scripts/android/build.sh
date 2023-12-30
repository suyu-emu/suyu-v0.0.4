#!/bin/bash -ex

# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

export NDK_CCACHE="$(which ccache)"
ccache -s

BUILD_FLAVOR="mainline"

BUILD_TYPE="release"
if [ "${GITHUB_REPOSITORY}" == "yuzu-emu/yuzu" ]; then
    BUILD_TYPE="relWithDebInfo"
fi

if [ ! -z "${ANDROID_KEYSTORE_B64}" ]; then
    export ANDROID_KEYSTORE_FILE="${GITHUB_WORKSPACE}/ks.jks"
    base64 --decode <<< "${ANDROID_KEYSTORE_B64}" > "${ANDROID_KEYSTORE_FILE}"
fi

cd src/android
chmod +x ./gradlew
./gradlew "assemble${BUILD_FLAVOR}${BUILD_TYPE}" "bundle${BUILD_FLAVOR}${BUILD_TYPE}"

ccache -s

if [ ! -z "${ANDROID_KEYSTORE_B64}" ]; then
    rm "${ANDROID_KEYSTORE_FILE}"
fi
