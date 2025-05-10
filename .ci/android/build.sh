#!/bin/bash -e

# SPDX-FileCopyrightText: 2025 eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

export NDK_CCACHE=$(which ccache)

# keystore & pass are stored locally
export ANDROID_KEYSTORE_FILE=~/android.keystore
export ANDROID_KEYSTORE_PASS=`cat ~/android.pass`
export ANDROID_KEY_ALIAS=`cat ~/android.alias`

cd src/android
chmod +x ./gradlew

./gradlew assembleRelease
./gradlew bundleRelease

ccache -s -v
