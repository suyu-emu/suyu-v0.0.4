#!/bin/sh -e
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

cat src/android/app/src/main/res/values/strings.xml \
    | grep 'string name="' | awk -F'"' '$0=$2' \
    | xargs -I {} sh -c 'grep -qirnw R.string.'{}' src/android/app/src || echo '{}
