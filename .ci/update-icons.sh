#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

which png2icns || [ which yay && yay libicns ] || exit
which magick || exit

export EDEN_SVG_ICO="dist/dev.eden_emu.eden.svg"
svgo --multipass $EDEN_SVG_ICO

magick -density 256x256 -background transparent $EDEN_SVG_ICO \
    -define icon:auto-resize -colors 256 dist/eden.ico || exit
convert -density 256x256 -resize 256x256 -background transparent $EDEN_SVG_ICO \
    dist/yuzu.bmp || exit

export TMP_PNG="dist/eden-tmp.png"
magick -size 1024x1024 -background transparent $EDEN_SVG_ICO $TMP_PNG || exit
png2icns dist/eden.icns $TMP_PNG || exit
cp dist/eden.icns dist/yuzu.icns
rm $TMP_PNG
