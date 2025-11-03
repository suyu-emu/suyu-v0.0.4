#!/usr/bin/env bash

# Robust vcpkg recovery and reinstall script
# - Cleans local build artifacts and vcpkg installation for the active triplet
# - Bootstraps vcpkg if needed and updates to latest master
# - Installs dependencies from this repo's vcpkg.json (manifest mode)
#
# Notes:
# - Respects VCPKG_DEFAULT_TRIPLET if set, otherwise defaults to x64-linux
# - Keeps the global downloads cache to avoid re-downloading large archives
# - Safe to re-run multiple times

set -euo pipefail

REPO_ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "${REPO_ROOT_DIR}"

VCPKG_ROOT=${VCPKG_ROOT:-"${REPO_ROOT_DIR}/externals/vcpkg"}
TRIPLET=${VCPKG_DEFAULT_TRIPLET:-x64-linux}

info()  { echo -e "[info]  $*"; }
warn()  { echo -e "[warn]  $*"; }
error() { echo -e "[error] $*" >&2; }

# Ensure vcpkg submodule exists
if [[ ! -d "${VCPKG_ROOT}" ]]; then
  info "Initializing vcpkg submodule..."
  git submodule update --init externals/vcpkg
fi

# Update vcpkg to latest master
if [[ -d "${VCPKG_ROOT}/.git" ]]; then
  info "Updating vcpkg..."
  git -C "${VCPKG_ROOT}" fetch origin
  git -C "${VCPKG_ROOT}" checkout master || true
  git -C "${VCPKG_ROOT}" pull --ff-only || git -C "${VCPKG_ROOT}" pull
else
  warn "vcpkg directory is not a git repository; skipping update"
fi

# Bootstrap vcpkg tool
if [[ ! -x "${VCPKG_ROOT}/vcpkg" ]]; then
  info "Bootstrapping vcpkg..."
  if [[ -f "${VCPKG_ROOT}/bootstrap-vcpkg.sh" ]]; then
    "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics
  else
    error "bootstrap-vcpkg.sh not found under ${VCPKG_ROOT}"
    exit 1
  fi
fi

info "Active triplet: ${TRIPLET}"

# Clean local build artifacts that often cause stale state
info "Cleaning local build artifacts..."
rm -rf "${REPO_ROOT_DIR}/builddir/vcpkg_installed" || true
rm -rf "${REPO_ROOT_DIR}/vcpkg_installed" || true

# Clean per-triplet vcpkg state to ensure a fresh, consistent build
# Keep downloads cache to avoid re-downloading all packages
if [[ -d "${VCPKG_ROOT}" ]]; then
  info "Cleaning vcpkg installed/buildtrees/packages for ${TRIPLET}..."
  rm -rf "${VCPKG_ROOT}/installed/${TRIPLET}" || true
  rm -rf "${VCPKG_ROOT}/buildtrees" || true
  rm -rf "${VCPKG_ROOT}/packages" || true
else
  error "VCPKG_ROOT does not exist: ${VCPKG_ROOT}"
  exit 1
fi

# Verify manifest exists
if [[ ! -f "${REPO_ROOT_DIR}/vcpkg.json" ]]; then
  error "vcpkg.json not found at repo root; cannot use manifest mode"
  exit 1
fi

# Ensure base meta-ports are available for CMake toolchain integration
info "Installing vcpkg meta-ports (vcpkg-cmake, vcpkg-cmake-config)..."
"${VCPKG_ROOT}/vcpkg" install \
  vcpkg-cmake vcpkg-cmake-config \
  --triplet="${TRIPLET}"

# Install all manifest dependencies for the active triplet
info "Installing manifest dependencies from vcpkg.json..."
"${VCPKG_ROOT}/vcpkg" install --triplet="${TRIPLET}"

info "Done. vcpkg dependencies are installed for ${TRIPLET}."