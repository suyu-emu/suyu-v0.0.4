#!/bin/sh

# SPDX-FileCopyrightText: 2025 eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# This script assumes you're in the source directory
set -ex

export APPIMAGE_EXTRACT_AND_RUN=1
export BASE_ARCH="$(uname -m)"
export ARCH="$BASE_ARCH"

LIB4BN="https://raw.githubusercontent.com/VHSgunzo/sharun/refs/heads/main/lib4bin"
URUNTIME="https://github.com/VHSgunzo/uruntime/releases/latest/download/uruntime-appimage-dwarfs-$ARCH"

if [ "$ARCH" = 'x86_64' ]; then
	if [ "$1" = 'v3' ]; then
		ARCH="${ARCH}_v3"
	fi
fi

EDEN_TAG=$(git describe --tags --abbrev=0)
echo "Making stable \"$EDEN_TAG\" build"
git checkout "$EDEN_TAG"
VERSION="$(echo "$EDEN_TAG")"

# NOW MAKE APPIMAGE
mkdir -p ./AppDir
cd ./AppDir

cat > org.eden_emu.eden.desktop << EOL
[Desktop Entry]
Type=Application
Name=Eden
Icon=org.eden_emu.eden
Exec=eden
Categories=Game;Emulator;
EOL

cp ../dist/eden.svg ./org.eden_emu.eden.svg

ln -sf ./org.eden_emu.eden.svg.svg ./.DirIcon

if [ "$DEVEL" = 'true' ]; then
	sed -i 's|Name=Eden|Name=Eden Nightly|' ./org.eden_emu.eden.desktop
	UPINFO="$(echo "$UPINFO" | sed 's|latest|nightly|')"
fi

LIBDIR="/usr/lib"
# some distros are weird and use a subdir

if [ ! -f "/usr/lib/libGL.so" ]
then
    LIBDIR="/usr/lib/${BASE_ARCH}-linux-gnu"
fi

# Bundle all libs
wget --retry-connrefused --tries=30 "$LIB4BN" -O ./lib4bin
chmod +x ./lib4bin
xvfb-run -a -- ./lib4bin -p -v -e -s -k \
	../build/bin/eden* \
	$LIBDIR/lib*GL*.so* \
    $LIBDIR/libSDL2*.so* \
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

# Prepare sharun
if [ "$ARCH" = 'aarch64' ]; then
	# allow the host vulkan to be used for aarch64 given the sed situation
	echo 'SHARUN_ALLOW_SYS_VKICD=1' > ./.env
fi

wget https://github.com/VHSgunzo/sharun/releases/download/v0.6.3/sharun-x86_64 -O sharun
chmod a+x sharun

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

# Cleanup

rm -rf AppDir
rm uruntime
