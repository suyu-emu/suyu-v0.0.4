@echo off
REM SPDX-FileCopyrightText: 2024 suyu Emulator Project
REM SPDX-License-Identifier: GPL-2.0-or-later

echo Cleaning boost components from vcpkg...
echo.

REM Check if vcpkg exists in externals directory
if exist "externals\vcpkg\vcpkg.exe" (
    echo Using bundled vcpkg from externals directory...
    set VCPKG_EXE=externals\vcpkg\vcpkg.exe
) else if exist "vcpkg\vcpkg.exe" (
    echo Using vcpkg from current directory...
    set VCPKG_EXE=vcpkg\vcpkg.exe
) else (
    echo Error: vcpkg.exe not found. Please ensure vcpkg is installed.
    echo Expected locations:
    echo   - externals\vcpkg\vcpkg.exe
    echo   - vcpkg\vcpkg.exe
    pause
    exit /b 1
)

echo Removing all boost components for x64-windows...
echo Command: %VCPKG_EXE% remove boost-uninstall:x64-windows --recurse
echo.

%VCPKG_EXE% remove boost-uninstall:x64-windows --recurse

if %ERRORLEVEL% neq 0 (
    echo.
    echo Warning: boost-uninstall command failed or no boost components were installed.
    echo This is normal if boost components were not previously installed.
) else (
    echo.
    echo Boost components successfully removed.
)

echo.
echo Cleaning completed. You can now run vcpkg install to reinstall dependencies.
pause