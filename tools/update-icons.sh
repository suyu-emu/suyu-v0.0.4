#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Updates main icons for eden

#which png2icns || (which yay && yay libicns) || exit
which magick || exit

EDEN_SVG_ICO="dist/dev.eden_emu.eden.svg"
EALT_SVG_ICO="dist/eden_named.svg"

magick -density 256x256 -background transparent $EDEN_SVG_ICO -define icon:auto-resize -colors 256 dist/eden.ico || exit
convert -density 256x256 -resize 256x256 -background transparent $EDEN_SVG_ICO dist/yuzu.bmp || exit

magick -size 256x256 -background transparent $EDEN_SVG_ICO dist/qt_themes/default/icons/256x256/eden.png || exit
magick -size 256x256 -background transparent $EALT_SVG_ICO dist/qt_themes/default/icons/256x256/eden_named.png || exit
magick dist/qt_themes/default/icons/256x256/eden.png -resize 256x256! dist/qt_themes/default/icons/256x256/eden.png || exit
magick dist/qt_themes/default/icons/256x256/eden_named.png -resize 256x256! dist/qt_themes/default/icons/256x256/eden_named.png || exit

# Now do more fancy things (like composition)
TMP_PNG="dist/eden-tmp.png"
magick -size 1024x1024 -background transparent $EDEN_SVG_ICO $TMP_PNG || exit
composite $TMP_PNG -gravity center -geometry 2048x2048+0+0 \
    src/android/app/src/main/res/drawable/ic_icon_bg_orig.png \
    src/android/app/src/main/res/drawable/ic_launcher.png || exit
magick src/android/app/src/main/res/drawable/ic_launcher.png -resize 512x512! src/android/app/src/main/res/drawable/ic_launcher.png || exit

optipng -o7 src/android/app/src/main/res/drawable/ic_launcher.png
optipng -o7 dist/qt_themes/default/icons/256x256/eden_named.png
optipng -o7 dist/qt_themes/default/icons/256x256/eden.png

png2icns dist/eden.icns $TMP_PNG
cp dist/eden.icns dist/yuzu.icns
rm $TMP_PNG
