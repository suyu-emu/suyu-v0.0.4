#!/bin/sh -e
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later
which optipng || exit
find . -type f -iname '*.png' -print0 | xargs -0 -P 16 -I {} optipng -o7 {}
