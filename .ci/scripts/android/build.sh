#!/bin/bash -ex

# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

export NDK_CCACHE="$(which ccache)"
ccache -s

BUILD_FLAVOR=mainline

cd src/android
chmod +x ./gradlew
./gradlew "assemble${BUILD_FLAVOR}Release" "bundle${BUILD_FLAVOR}Release"

ccache -s
