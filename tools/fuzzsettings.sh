#!/bin/sh -ex

# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

ROOTDIR=$(CDPATH='' cd -- "$(dirname -- "$0")/" && pwd)

touch "$2"

c++ "$ROOTDIR/fuzzsettings.cpp" -o fuzzsettings
./fuzzsettings "$1" >"$2"
rm fuzzsettings
