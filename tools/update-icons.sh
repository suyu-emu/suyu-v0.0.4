#!/bin/sh -ex

# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Updates main icons for eden

which magick || exit
which optipng || exit

VARIATION=${VARIATION:-base}

EDEN_BASE_SVG="dist/icon_variations/${VARIATION}.svg"
EDEN_BG_COLOR="dist/icon_variations/${VARIATION}_bgcolor"
# TODO: EDEN_MONOCHROME_SVG Variation

[ -f "$EDEN_BASE_SVG" ] && [ -f "$EDEN_BG_COLOR" ] || { echo "Error: missing ${VARIATION}.svg/${VARIATION}_bgcolor" >&2; exit; }

# Desktop / Windows / Qt icons

EDEN_DESKTOP_SVG="dist/dev.eden_emu.eden.svg"

cp "$EDEN_BASE_SVG" "$EDEN_DESKTOP_SVG"

magick -density 256x256 -background transparent "$EDEN_BASE_SVG" -define icon:auto-resize -colors 256 dist/eden.ico || exit
magick -density 256x256 -background transparent "$EDEN_BASE_SVG" -resize 256x256 dist/eden.bmp || exit

magick -size 256x256 -background transparent "$EDEN_BASE_SVG" -resize 256x256 dist/qt_themes/default/icons/256x256/eden.png || exit

optipng -o7 dist/qt_themes/default/icons/256x256/eden.png

# Android adaptive icon (API 26+)

EDEN_ANDROID_RES="src/android/app/src/main/res"
EDEN_ANDROID_FG="$EDEN_ANDROID_RES/drawable/ic_launcher_foreground.png"
EDEN_ANDROID_BG_COLOR=$(cat $EDEN_BG_COLOR)

# Update Icon Background Color
echo "<?xml version='1.0' encoding='utf-8'?><resources><color name='ic_launcher_background'>${EDEN_ANDROID_BG_COLOR}</color></resources>" > "$EDEN_ANDROID_RES/values/colors.xml"

magick -size 1080x1080 -background transparent "$EDEN_BASE_SVG" -gravity center -resize 660x660 -extent 1080x1080 "$EDEN_ANDROID_FG" || exit
magick -background transparent "$EDEN_BASE_SVG" -gravity center -resize 512x512 "$EDEN_ANDROID_RES/drawable/ic_yuzu.png" || exit
magick -size 512x512 -background transparent "$EDEN_BASE_SVG" -gravity center -resize 338x338 -extent 512x512 "$EDEN_ANDROID_RES/drawable/ic_yuzu_splash.png" || exit

optipng -o7 "$EDEN_ANDROID_FG"
optipng -o7 "$EDEN_ANDROID_RES/drawable/ic_yuzu.png"
optipng -o7 "$EDEN_ANDROID_RES/drawable/ic_yuzu_splash.png"

# Android legacy launcher icon (API <= 24)

BASE_LEGACY="$EDEN_ANDROID_RES/mipmap-xxxhdpi/ic_launcher.png"

magick -size 512x512 xc:${EDEN_ANDROID_BG_COLOR} "$EDEN_ANDROID_FG" -gravity center -resize 384x384 -composite "$BASE_LEGACY" || exit

magick "$BASE_LEGACY" -resize 192x192 "$EDEN_ANDROID_RES/mipmap-xxhdpi/ic_launcher.png"
magick "$BASE_LEGACY" -resize 144x144 "$EDEN_ANDROID_RES/mipmap-xhdpi/ic_launcher.png"
magick "$BASE_LEGACY" -resize 96x96 "$EDEN_ANDROID_RES/mipmap-hdpi/ic_launcher.png"
magick "$BASE_LEGACY" -resize 72x72 "$EDEN_ANDROID_RES/mipmap-mdpi/ic_launcher.png"

optipng -o7 "$EDEN_ANDROID_RES"/mipmap-*/ic_launcher.png

# macOS
# TODO: Update Assets.car too

TMP_PNG="dist/eden-tmp.png"

magick -size 1024x1024 -background none "$EDEN_BASE_SVG" "$TMP_PNG" || exit

png2icns dist/eden.icns "$TMP_PNG" || echo 'non fatal'

rm "$TMP_PNG"
