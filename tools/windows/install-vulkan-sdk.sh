#!/usr/bin/sh
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

: "${VULKAN_SDK_VER:=1.4.328.1}"
: "${VULKAN_ROOT:=C:/VulkanSDK/$VULKAN_SDK_VER}"
EXE_FILE="vulkansdk-windows-X64-$VULKAN_SDK_VER.exe"
URI="https://sdk.lunarg.com/sdk/download/$VULKAN_SDK_VER/windows/$EXE_FILE"
VULKAN_ROOT_UNIX=$(cygpath -u "$VULKAN_ROOT")

# Check if Vulkan SDK is already installed
if [ -d "$VULKAN_ROOT_UNIX" ]; then
    echo "-- Vulkan SDK already installed at $VULKAN_ROOT_UNIX"
    exit 0
fi

echo "Downloading Vulkan SDK $VULKAN_SDK_VER from $URI"
[ ! -f "./$EXE_FILE" ] && curl -L -o "./$EXE_FILE" "$URI"
chmod +x "./$EXE_FILE"
echo "Finished downloading $EXE_FILE"

echo "Installing Vulkan SDK $VULKAN_SDK_VER..."
if net session > /dev/null 2>&1; then
    ./$EXE_FILE --root "$VULKAN_ROOT" --accept-licenses --default-answer --confirm-command install
else
    echo "This script must be run with administrator privileges!"
    exit 1
fi

echo "Finished installing Vulkan SDK $VULKAN_SDK_VER"

# GitHub Actions integration
if [ \"${GITHUB_ACTIONS:-false}\" = \"true\" ]; then
    echo \"VULKAN_SDK=$VULKAN_ROOT\" >> \"$GITHUB_ENV\"
    echo \"$VULKAN_ROOT/bin\" >> \"$GITHUB_PATH\"
fi
