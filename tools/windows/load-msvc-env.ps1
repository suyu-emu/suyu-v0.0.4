# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

$osArch = $env:PROCESSOR_ARCHITECTURE

switch ($osArch) {
    "AMD64" { $arch = "x64" }
    "ARM64" { $arch = "arm64" }
    default {
        Write-Error "load-msvc-env.ps1: Unsupported architecture: $osArch"
        exit 1
    }
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (!(Test-Path $vswhere)) {
    Write-Error "load-msvc-env.ps1: vswhere not found"
    exit 1
}

$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath

if (-not $vs) {
    Write-Error "load-msvc-env.ps1: Visual Studio (with Desktop development with C++) not found"
    exit 1
}

$bat = "$vs\VC\Auxiliary\Build\vcvarsall.bat"

if (!(Test-Path $bat)) {
    Write-Error "load-msvc-env.ps1: (vcvarsall.bat) not found"
    exit 1
}

cmd /c "`"$bat`" $arch && set" | ForEach-Object {
    if ($_ -match "^(.*?)=(.*)$") {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}

Write-Host "load-msvc-env.ps1: MSVC environment loaded for $arch ($vs)"
