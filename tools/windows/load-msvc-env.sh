#!/usr/bin/bash
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

ARCH_RAW="$PROCESSOR_ARCHITECTURE"

case "$ARCH_RAW" in
    AMD64) ARCH="x64" ;;
    ARM64) ARCH="arm64" ;;
    *) echo "load-msvc-env.sh: Unsupported architecture: $ARCH_RAW"; exit 1 ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VCVARS_BASH="$SCRIPT_DIR/vcvarsall.sh"

if [ ! -f "$VCVARS_BASH" ]; then
    echo "load-msvc-env.sh: vcvarsall.sh not found in $SCRIPT_DIR"
    #exit 1
fi
chmod +x "$VCVARS_BASH"

eval "$("$VCVARS_BASH" "$ARCH")"

echo "MSVC environment loaded for $ARCH with vcvars-bash script"