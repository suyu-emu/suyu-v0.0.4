# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

$ErrorActionPreference = "Stop"

$VulkanSDKVer = "1.3.250.1"
$ExeFile = "VulkanSDK-$VulkanSDKVer-Installer.exe"
$Uri = "https://sdk.lunarg.com/sdk/download/$VulkanSDKVer/windows/$ExeFile"
$Destination = "./$ExeFile"

echo "Downloading Vulkan SDK $VulkanSDKVer from $Uri"
$WebClient = New-Object System.Net.WebClient
$WebClient.DownloadFile($Uri, $Destination)
echo "Finished downloading $ExeFile"

$VULKAN_SDK = "C:/VulkanSDK/$VulkanSDKVer"
$Arguments = "--root `"$VULKAN_SDK`" --accept-licenses --default-answer --confirm-command install"

echo "Installing Vulkan SDK $VulkanSDKVer"
$InstallProcess = Start-Process -FilePath $Destination -NoNewWindow -PassThru -Wait -ArgumentList $Arguments
$ExitCode = $InstallProcess.ExitCode

if ($ExitCode -ne 0) {
    echo "Error installing Vulkan SDK $VulkanSDKVer (Error: $ExitCode)"
    Exit $ExitCode
}

echo "Finished installing Vulkan SDK $VulkanSDKVer"

if ("$env:GITHUB_ACTIONS" -eq "true") {
    echo "VULKAN_SDK=$VULKAN_SDK" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
    echo "$VULKAN_SDK/Bin" | Out-File -FilePath $env:GITHUB_PATH -Encoding utf8 -Append
}
