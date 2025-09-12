# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

$ErrorActionPreference = "Stop"

# Check if running as administrator
if (-not ([bool](net session 2>$null))) {
    Write-Host "This script must be run with administrator privileges!"
    Exit 1
}

$VulkanSDKVer = "1.4.321.1"
$ExeFile = "vulkansdk-windows-X64-$VulkanSDKVer.exe"
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