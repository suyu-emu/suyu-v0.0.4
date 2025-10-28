#!/bin/sh

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

for i in dist/languages/*.ts; do
    SRC=en_US
    TARGET=`head -n1 $i | awk -F 'language="' '{split($2, a, "\""); print a[1]}'`

    # requires fd
    SOURCES=`fd . src/yuzu src/qt_common -tf -e ui -e cpp -e h -e plist`

    lupdate -source-language $SRC -target-language $TARGET $SOURCES -ts /data/code/eden/$i
done
