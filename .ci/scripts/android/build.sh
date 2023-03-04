#!/bin/bash -ex

export NDK_CCACHE="$(which ccache)"
ccache -s

BUILD_FLAVOR=mainline

cd src/android
chmod +x ./gradlew
./gradlew "assemble${BUILD_FLAVOR}Release" "bundle${BUILD_FLAVOR}Release"

ccache -s
