#!/bin/bash -ex

export NDK_CCACHE=$(which ccache)

# keystore & pass are stored locally
export ANDROID_KEYSTORE_FILE=~/android.keystore
export ANDROID_KEYSTORE_PASS=`cat ~/android.pass`
export ANDROID_KEY_ALIAS=`cat ~/android.alias`
export ANDROID_HOME=/opt/android-sdk/

cd src/android
chmod +x ./gradlew

./gradlew assembleRelease
./gradlew bundleRelease

ccache -s -v
