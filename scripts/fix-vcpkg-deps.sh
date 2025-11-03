#!/bin/bash

# Exit on error
set -e

echo "Cleaning vcpkg build files..."
rm -rf builddir/vcpkg_installed || true
rm -rf vcpkg_installed || true

echo "Updating vcpkg..."
git -C externals/vcpkg pull origin master

echo "Bootstrap vcpkg..."
externals/vcpkg/bootstrap-vcpkg.sh -disableMetrics

echo "Installing base dependencies..."
./externals/vcpkg/vcpkg install vcpkg-cmake vcpkg-cmake-config --triplet=x64-linux

echo "Installing project dependencies..."
./externals/vcpkg/vcpkg install --triplet=x64-linux