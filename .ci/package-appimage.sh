#!/bin/sh

# This script assumes you're in the source directory
set -ex

export APPIMAGE_EXTRACT_AND_RUN=1
export ARCH="$(uname -m)"

LIB4BN="https://raw.githubusercontent.com/VHSgunzo/sharun/refs/heads/main/lib4bin"
URUNTIME="https://github.com/VHSgunzo/uruntime/releases/latest/download/uruntime-appimage-dwarfs-$ARCH"

if [ "$ARCH" = 'x86_64' ]; then
	if [ "$1" = 'v3' ]; then
		ARCH="${ARCH}_v3"
	fi
fi

#if [ "$DEVEL" = 'true' ]; then
#    YUZU_TAG="$(git rev-parse --short HEAD)"
#    echo "Making nightly \"$YUZU_TAG\" build"
#    VERSION="$YUZU_TAG"
#else
#    YUZU_TAG=$(git describe --tags)
#    echo "Making stable \"$YUZU_TAG\" build"
#    git checkout "$YUZU_TAG"
#    VERSION="$(echo "$YUZU_TAG" | awk -F'-' '{print $1}')"
#fi

# TODO: use real tags
YUZU_TAG="0.0.0"

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

# Bundle all libs
wget --retry-connrefused --tries=30 "$LIB4BN" -O ./lib4bin
chmod +x ./lib4bin
xvfb-run -a -- ./lib4bin -p -v -e -s -k \
	../build/bin/eden* \
	/usr/lib/libGLX* \
	/usr/lib/libGL.so* \
	/usr/lib/libEGL* \
	/usr/lib/dri/* \
	/usr/lib/vdpau/* \
	/usr/lib/libvulkan* \
	/usr/lib/libXss.so* \
	/usr/lib/libdecor-0.so* \
	/usr/lib/libgamemode.so* \
	/usr/lib/qt6/plugins/audio/* \
	/usr/lib/qt6/plugins/bearer/* \
	/usr/lib/qt6/plugins/imageformats/* \
	/usr/lib/qt6/plugins/iconengines/* \
	/usr/lib/qt6/plugins/platforms/* \
	/usr/lib/qt6/plugins/platformthemes/* \
	/usr/lib/qt6/plugins/platforminputcontexts/* \
	/usr/lib/qt6/plugins/styles/* \
	/usr/lib/qt6/plugins/xcbglintegrations/* \
	/usr/lib/qt6/plugins/wayland-*/* \
	/usr/lib/pulseaudio/* \
	/usr/lib/pipewire-0.3/* \
	/usr/lib/spa-0.2/*/* \
	/usr/lib/alsa-lib/*

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
	--categorize=hotness --hotness-list=.ci/eden.dwfsprof \
	--compression zstd:level=22 -S26 -B32 \
	--header uruntime \
	-i ./AppDir -o Eden-"$VERSION"-"$ARCH".AppImage

echo "Generating zsync file..."
zsyncmake *.AppImage -u *.AppImage
echo "All Done!"

# Cleanup

rm -rf AppDir
rm uruntime
