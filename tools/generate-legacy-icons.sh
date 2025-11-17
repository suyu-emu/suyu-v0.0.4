#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Generate SDK <26 icons for android
# requires imagemagick, inkscape

ROOTDIR=$PWD

cd src/android/app/src/main

cd res/drawable
# convert vector to svg--needed to generate launcher png
cp ic_yuzu_icon.xml tmp

python3 "$ROOTDIR"/tools/VectorDrawable2Svg.py tmp

inkscape -w 768 -h 768 tmp.svg -o ic_tmp.png
magick ic_icon_bg_orig.png -resize 512x512 bg_tmp.png

magick bg_tmp.png -strip -type TrueColor -depth 8 -colorspace sRGB -color-matrix "1 0 0 0 0 0 1 0 0 0 0 1 0 0 0 0" bg_tmp_rgb.png
magick -verbose bg_tmp_rgb.png ic_tmp.png -gravity center -composite -colorspace sRGB ic_launcher.png
echo

rm ./*tmp*

cd "$ROOTDIR"

# TODO: add legacy icons
