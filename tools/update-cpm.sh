#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# updates CPMUtil, its docs, and related tools from the latest release

if command -v zstd > /dev/null; then
    EXT=tar.zst
else
    EXT=tar.gz
fi

wget "https://git.crueter.xyz/CMake/CPMUtil/releases/download/continuous/CPMUtil.$EXT"
tar xf "CPMUtil.$EXT"
rm "CPMUtil.$EXT"
