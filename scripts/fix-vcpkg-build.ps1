# SPDX-FileCopyrightText: 2024 suyu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Fix vcpkg build issues with boost-cmake and missing vcpkg-cmake configuration files

Write-Host "=== Suyu vcpkg Build Fix Script ===" -ForegroundColor Green
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
Write-Host "Step 1: Cleaning existing boost installations..." -ForegroundColor Cyan
Write-Host "Command: $vcpkgExe remove boost-uninstall:x64-windows --recurse" -ForegroundColor Gray

try {
    & $vcpkgExe remove boost-uninstall:x64-windows --recurse
    Write-Host "Boost cleanup completed." -ForegroundColor Green
} catch {
    Write-Host "Warning: Boost cleanup failed or no boost components were installed." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Step 2: Cleaning vcpkg cache and buildtrees..." -ForegroundColor Cyan

$vcpkgDir = Split-Path $vcpkgExe -Parent
$buildtreesPath = Join-Path $vcpkgDir "buildtrees"
$installedPath = Join-Path $vcpkgDir "installed\x64-windows"

if (Test-Path $buildtreesPath) {
    Write-Host "Removing buildtrees directory..." -ForegroundColor Gray
    Remove-Item -Recurse -Force $buildtreesPath -ErrorAction SilentlyContinue
}

if (Test-Path $installedPath) {
    Write-Host "Removing installed x64-windows directory..." -ForegroundColor Gray
    Remove-Item -Recurse -Force $installedPath -ErrorAction SilentlyContinue
}

Write-Host "Cache cleanup completed." -ForegroundColor Green

Write-Host ""
Write-Host "Step 3: Installing dependencies with proper order..." -ForegroundColor Cyan
Write-Host "This will install vcpkg-cmake tools first, then other dependencies." -ForegroundColor Gray
Write-Host ""

# Check if vcpkg.json exists
if (-not (Test-Path "vcpkg.json")) {
    Write-Host "Error: vcpkg.json not found in current directory." -ForegroundColor Red
    Write-Host "Please run this script from the project root directory." -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "Installing packages from vcpkg.json..." -ForegroundColor Cyan
Write-Host "Command: $vcpkgExe install --triplet x64-windows --clean-after-build" -ForegroundColor Gray

try {
    & $vcpkgExe install --triplet x64-windows --clean-after-build
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "=== SUCCESS ===" -ForegroundColor Green
        Write-Host "All dependencies installed successfully!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Next steps:" -ForegroundColor Yellow
        Write-Host "1. Configure CMake: cmake -B build -DCMAKE_TOOLCHAIN_FILE=$vcpkgDir/scripts/buildsystems/vcpkg.cmake" -ForegroundColor White
        Write-Host "2. Build project: cmake --build build --config Release" -ForegroundColor White
    } else {
        Write-Host ""
        Write-Host "=== INSTALLATION FAILED ===" -ForegroundColor Red
        Write-Host "vcpkg install returned exit code: $LASTEXITCODE" -ForegroundColor Red
        Write-Host ""
        Write-Host "Troubleshooting steps:" -ForegroundColor Yellow
        Write-Host "1. Check the error messages above" -ForegroundColor White
        Write-Host "2. Verify vcpkg baseline is up to date" -ForegroundColor White
        Write-Host "3. Try running: $vcpkgExe update" -ForegroundColor White
        Write-Host "4. Check vcpkg-configuration.json for correct baseline" -ForegroundColor White
    }
} catch {
    Write-Host ""
    Write-Host "Error executing vcpkg install: $_" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== Build Fix Script Completed ===" -ForegroundColor Green
Read-Host "Press Enter to exit"