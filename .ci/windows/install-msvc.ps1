# SPDX-FileCopyrightText: 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

$ErrorActionPreference = "Stop"

# Check if running as administrator
if (-not ([bool](net session 2>$null))) {
    Write-Host "This script must be run with administrator privileges!"
    Exit 1
}

$VSVer = "17"
$ExeFile = "vs_BuildTools.exe"
$Uri = "https://aka.ms/vs/$VSVer/release/$ExeFile"
$Destination = "./$ExeFile"

Write-Host "Downloading Visual Studio Build Tools from $Uri"
$WebClient = New-Object System.Net.WebClient
$WebClient.DownloadFile($Uri, $Destination)
Write-Host "Finished downloading $ExeFile"

$VSROOT = "C:/VSBuildTools/$VSVer"
$Arguments = @(
    "--installPath `"$VSROOT`"",                               # set custom installation path
    "--quiet",                                                  # suppress UI
    "--wait",                                                   # wait for installation to complete
    "--norestart",                                              # prevent automatic restart
    "--add Microsoft.VisualStudio.Workload.VCTools",            # add C++ build tools workload
    "--add Microsoft.VisualStudio.Component.VC.Tools.x86.x64",  # add core x86/x64 C++ tools
    "--add Microsoft.VisualStudio.Component.Windows10SDK.19041" # add specific Windows SDK
)

Write-Host "Installing Visual Studio Build Tools"
$InstallProcess = Start-Process -FilePath $Destination -NoNewWindow -PassThru -Wait -ArgumentList $Arguments
$ExitCode = $InstallProcess.ExitCode

if ($ExitCode -ne 0) {
    Write-Host "Error installing Visual Studio Build Tools (Error: $ExitCode)"
    Exit $ExitCode
}

Write-Host "Finished installing Visual Studio Build Tools"
