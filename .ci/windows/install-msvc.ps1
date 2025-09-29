# SPDX-FileCopyrightText: 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

$ErrorActionPreference = "Stop"

# Check if running as administrator
if (-not ([bool](net session 2>$null))) {
    Write-Host "This script must be run with administrator privileges!"
    Exit 1
}

$VSVer = "17"
$ExeFile = "vs_community.exe"
$Uri = "https://aka.ms/vs/$VSVer/release/$ExeFile"
$Destination = "./$ExeFile"

Write-Host "Downloading Visual Studio Build Tools from $Uri"
$WebClient = New-Object System.Net.WebClient
$WebClient.DownloadFile($Uri, $Destination)
Write-Host "Finished downloading $ExeFile"

$Arguments = @(
    "--quiet",                                                  # Suppress installer UI
    "--wait",                                                   # Wait for installation to complete
    "--norestart",                                              # Prevent automatic restart
    "--force",                                                  # Force installation even if components are already installed
    "--add Microsoft.VisualStudio.Workload.NativeDesktop",      # Desktop development with C++
    "--add Microsoft.VisualStudio.Component.VC.Tools.x86.x64",  # Core C++ compiler/tools for x86/x64
    "--add Microsoft.VisualStudio.Component.Windows11SDK.26100",# Windows 11 SDK (26100)
    "--add Microsoft.VisualStudio.Component.Windows10SDK.19041",# Windows 10 SDK (19041)
    "--add Microsoft.VisualStudio.Component.VC.Llvm.Clang",     # LLVM Clang compiler
    "--add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset", # LLVM Clang integration toolset
    "--add Microsoft.VisualStudio.Component.Windows11SDK.22621",# Windows 11 SDK (22621)
    "--add Microsoft.VisualStudio.Component.VC.CMake.Project",  # CMake project support
    "--add Microsoft.VisualStudio.ComponentGroup.VC.Tools.142.x86.x64", # VC++ 14.2 toolset
    "--add Microsoft.VisualStudio.ComponentGroup.NativeDesktop.Llvm.Clang" # LLVM Clang for native desktop
)

Write-Host "Installing Visual Studio Build Tools"
$InstallProcess = Start-Process -FilePath $Destination -NoNewWindow -PassThru -ArgumentList $Arguments

# Spinner while installing
$Spinner = "|/-\"
$i = 0
while (-not $InstallProcess.HasExited) {
    Write-Host -NoNewline ("`rInstalling... " + $Spinner[$i % $Spinner.Length])
    Start-Sleep -Milliseconds 250
    $i++
}

# Clear spinner line
Write-Host "`rSetup completed!           "

$ExitCode = $InstallProcess.ExitCode
if ($ExitCode -ne 0) {
    Write-Host "Error installing Visual Studio Build Tools (Error: $ExitCode)"
    Exit $ExitCode
}

Write-Host "Finished installing Visual Studio Build Tools"
