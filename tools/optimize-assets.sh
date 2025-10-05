#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Optimizes assets of Eden (requires OptiPng)

which optipng || exit
find . -type f -name "*.png" -exec optipng -o7 {} \;
