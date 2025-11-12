#!/bin/sh

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

SOURCES=$(find src/yuzu src/qt_common -type f \( -name "*.ui" -o -name "*.cpp" -o -name "*.h" -o -name "*.plist" \))

# shellcheck disable=SC2086
lupdate -source-language en_US -target-language en_US $SOURCES -ts dist/languages/en.ts