#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# This script assumes you're in the source directory

export APPIMAGE_EXTRACT_AND_RUN=1
export BASE_ARCH="$(uname -m)"

SHARUN="https://github.com/VHSgunzo/sharun/releases/latest/download/sharun-${BASE_ARCH}-aio"
URUNTIME="https://github.com/VHSgunzo/uruntime/releases/latest/download/uruntime-appimage-dwarfs-${BASE_ARCH}"

case "$1" in
    amd64|"")
        echo "Packaging amd64-v3 optimized build of Eden"
        ARCH="amd64_v3"
        ;;
    steamdeck|zen2)
        echo "Packaging Steam Deck (Zen 2) optimized build of Eden"
        ARCH="steamdeck"
        ;;
    rog-ally|allyx|zen4)
        echo "Packaging ROG Ally X (Zen 4) optimized build of Eden"
        ARCH="rog-ally-x"
        ;;
    legacy)
        echo "Packaging amd64 generic build of Eden"
        ARCH=amd64
        ;;
    aarch64)
        echo "Packaging armv8-a build of Eden"
        ARCH=aarch64
        ;;
    armv9)
        echo "Packaging armv9-a build of Eden"
        ARCH=armv9
        ;;
		native)
        echo "Packaging native build of Eden"
        ARCH="$BASE_ARCH"
        ;;

esac

export BUILDDIR="$2"

if [ "$BUILDDIR" = '' ]
then
	BUILDDIR=build
fi

EDEN_TAG=$(git describe --tags --abbrev=0)
echo "Making \"$EDEN_TAG\" build"
# git checkout "$EDEN_TAG"
VERSION="$(echo "$EDEN_TAG")"

# NOW MAKE APPIMAGE
mkdir -p ./AppDir
cd ./AppDir

cp ../dist/dev.eden_emu.eden.desktop .
cp ../dist/dev.eden_emu.eden.svg .

ln -sf ./dev.eden_emu.eden.svg ./.DirIcon

UPINFO='gh-releases-zsync|eden-emulator|Releases|latest|*.AppImage.zsync'

if [ "$DEVEL" = 'true' ]; then
	sed -i 's|Name=Eden|Name=Eden Nightly|' ./dev.eden_emu.eden.desktop
 	UPINFO="$(echo "$UPINFO" | sed 's|Releases|nightly|')"
fi

LIBDIR="/usr/lib"

# Workaround for Gentoo
if [ ! -d "$LIBDIR/qt6" ]
then
	LIBDIR="/usr/lib64"
fi

# Workaround for Debian
if [ ! -d "$LIBDIR/qt6" ]
then
    LIBDIR="/usr/lib/${BASE_ARCH}-linux-gnu"
fi

# Bundle all libs

wget --retry-connrefused --tries=30 "$SHARUN" -O ./sharun-aio
chmod +x ./sharun-aio
xvfb-run -a ./sharun-aio l -p -v -e -s -k \
	../$BUILDDIR/bin/eden* \
	$LIBDIR/lib*GL*.so* \
	$LIBDIR/dri/* \
	$LIBDIR/vdpau/* \
	$LIBDIR/libvulkan* \
	$LIBDIR/libXss.so* \
	$LIBDIR/libdecor-0.so* \
	$LIBDIR/libgamemode.so* \
	$LIBDIR/qt6/plugins/audio/* \
	$LIBDIR/qt6/plugins/bearer/* \
	$LIBDIR/qt6/plugins/imageformats/* \
	$LIBDIR/qt6/plugins/iconengines/* \
	$LIBDIR/qt6/plugins/platforms/* \
	$LIBDIR/qt6/plugins/platformthemes/* \
	$LIBDIR/qt6/plugins/platforminputcontexts/* \
	$LIBDIR/qt6/plugins/styles/* \
	$LIBDIR/qt6/plugins/xcbglintegrations/* \
	$LIBDIR/qt6/plugins/wayland-*/* \
	$LIBDIR/pulseaudio/* \
	$LIBDIR/pipewire-0.3/* \
	$LIBDIR/spa-0.2/*/* \
	$LIBDIR/alsa-lib/*

rm -f ./sharun-aio

# Prepare sharun
if [ "$ARCH" = 'aarch64' ]; then
	# allow the host vulkan to be used for aarch64 given the sad situation
	echo 'SHARUN_ALLOW_SYS_VKICD=1' > ./.env
fi

# Workaround for Gentoo
if [ -d "shared/libproxy" ]; then
	cp shared/libproxy/* lib/
fi

ln -f ./sharun ./AppRun
./sharun -g

# turn appdir into appimage
cd ..
wget -q "$URUNTIME" -O ./uruntime
chmod +x ./uruntime

#Add udpate info to runtime
echo "Adding update information \"$UPINFO\" to runtime..."
./uruntime --appimage-addupdinfo "$UPINFO"

echo "Generating AppImage..."
./uruntime --appimage-mkdwarfs -f \
	--set-owner 0 --set-group 0 \
	--no-history --no-create-timestamp \
	--categorize=hotness --hotness-list=.ci/linux/eden.dwfsprof \
	--compression zstd:level=22 -S26 -B32 \
	--header uruntime \
    -N 4 \
	-i ./AppDir -o Eden-"$VERSION"-"$ARCH".AppImage

echo "Generating zsync file..."
zsyncmake *.AppImage -u *.AppImage
echo "All Done!"