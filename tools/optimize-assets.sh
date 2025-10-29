#!/bin/sh -e
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later
which optipng || exit
NPROC=$(nproc)
[ -z "$NPROC" ] && NPROC=8
find . -type f -iname '*.png' | xargs -P $NPROC -I {} optipng -o7 {}
