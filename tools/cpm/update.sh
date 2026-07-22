#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# updates CPMUtil, its docs, and related tools from the latest release

if command -v zstd >/dev/null 2>&1; then
	EXT=tar.zst
else
	EXT=tar.gz
fi

file=CPMUtil.$EXT
url="https://git.crueter.xyz/CMake/CPMUtil/releases/download/continuous/$file"

curl -L "$url" -o "$file"
tar xf "$file"
rm "$file"
