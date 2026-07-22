#!/bin/bash -ex

# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-FileCopyrightText: 2024 suyu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Ensure ccache is set up correctly
export NDK_CCACHE="$(which ccache)"
if [ -z "$NDK_CCACHE" ]; then
    echo "Error: ccache is not installed or not found in PATH"
    exit 1
fi
ccache -s

# Update git submodules
git submodule update --init --recursive

# Set build flavor and type
BUILD_FLAVOR="mainline"
BUILD_TYPE="release"
if [ "${GITHUB_REPOSITORY}" == "suyu/suyu" ]; then
    BUILD_TYPE="relWithDebInfo"
fi

# Handle Android Keystore
if [ ! -z "${ANDROID_KEYSTORE_B64}" ]; then
    export ANDROID_KEYSTORE_FILE="${GITHUB_WORKSPACE}/ks.jks"

    # Decode and save the keystore file
    if ! echo "${ANDROID_KEYSTORE_B64}" | base64 --decode > "${ANDROID_KEYSTORE_FILE}"; then
        echo "Error: Failed to decode ANDROID_KEYSTORE_B64"
        exit 1
    fi
else
    echo "Warning: ANDROID_KEYSTORE_B64 is not set. Proceeding without the keystore."
fi

# Navigate to the Android project directory
cd src/android || { echo "Error: Directory 'src/android' does not exist"; exit 1; }

# Ensure Gradle wrapper is executable
chmod +x ./gradlew

# Build APK and bundle
if ! ./gradlew "assemble${BUILD_FLAVOR}${BUILD_TYPE}" "bundle${BUILD_FLAVOR}${BUILD_TYPE}"; then
    echo "Error: Gradle build failed"
    exit 1
fi

# Display ccache statistics post-build
ccache -s

# Clean up keystore file if it was created
if [ ! -z "${ANDROID_KEYSTORE_B64}" ]; then
    rm -f "${ANDROID_KEYSTORE_FILE}"
fi

echo "Build completed successfully!"
