# SPDX-FileCopyrightText: 2024 suyu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

Write-Host "Cleaning boost components from vcpkg..." -ForegroundColor Green
Write-Host ""

# Check if vcpkg exists in various locations
$vcpkgPaths = @(
    "externals\vcpkg\vcpkg.exe",
    "vcpkg\vcpkg.exe",
    "..\vcpkg\vcpkg.exe"
)

$vcpkgExe = $null
foreach ($path in $vcpkgPaths) {
    if (Test-Path $path) {
        $vcpkgExe = $path
        Write-Host "Using vcpkg from: $path" -ForegroundColor Yellow
        break
    }
}

if (-not $vcpkgExe) {
    Write-Host "Error: vcpkg.exe not found." -ForegroundColor Red
    Write-Host "Expected locations:" -ForegroundColor Red
    foreach ($path in $vcpkgPaths) {
        Write-Host "  - $path" -ForegroundColor Red
    }
    Write-Host ""
    Write-Host "Please ensure vcpkg is installed and accessible." -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host ""
Write-Host "Removing all boost components for x64-windows..." -ForegroundColor Cyan
Write-Host "Command: $vcpkgExe remove boost-uninstall:x64-windows --recurse" -ForegroundColor Gray
Write-Host ""

try {
    & $vcpkgExe remove boost-uninstall:x64-windows --recurse
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "Boost components successfully removed." -ForegroundColor Green
    } else {
        Write-Host ""
        Write-Host "Warning: boost-uninstall command failed or no boost components were installed." -ForegroundColor Yellow
        Write-Host "This is normal if boost components were not previously installed." -ForegroundColor Yellow
    }
} catch {
    Write-Host ""
    Write-Host "Error executing vcpkg command: $_" -ForegroundColor Red
}

Write-Host ""
Write-Host "Cleaning completed. You can now run vcpkg install to reinstall dependencies." -ForegroundColor Green
Read-Host "Press Enter to exit"