#!/bin/bash -ex

cd /yuzu

ccache -s

mkdir build || true && cd build
cmake .. -G Ninja -DDISPLAY_VERSION=$1 -DCMAKE_TOOLCHAIN_FILE="$(pwd)/../CMakeModules/MinGWCross.cmake" -DUSE_CCACHE=ON -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON -DCMAKE_BUILD_TYPE=Release -DENABLE_QT_TRANSLATION=ON
ninja

ccache -s

echo "Tests skipped"
#ctest -VV -C Release

echo 'Prepare binaries...'
cd ..
mkdir package

if [ -d "/usr/x86_64-w64-mingw32/lib/qt5/plugins/platforms/" ]; then
  QT_PLATFORM_DLL_PATH='/usr/x86_64-w64-mingw32/lib/qt5/plugins/platforms/'
else
  #fallback to qt
  QT_PLATFORM_DLL_PATH='/usr/x86_64-w64-mingw32/lib/qt/plugins/platforms/'
fi

find build/ -name "yuzu*.exe" -exec cp {} 'package' \;

# copy Qt plugins
mkdir package/platforms
cp "${QT_PLATFORM_DLL_PATH}/qwindows.dll" package/platforms/
cp -rv "${QT_PLATFORM_DLL_PATH}/../mediaservice/" package/
cp -rv "${QT_PLATFORM_DLL_PATH}/../imageformats/" package/
rm -f package/mediaservice/*d.dll

for i in package/*.exe; do
  # we need to process pdb here, however, cv2pdb
  # does not work here, so we just simply strip all the debug symbols
  x86_64-w64-mingw32-strip "${i}"
done

pip3 install pefile
python3 .ci/scripts/windows/scan_dll.py package/*.exe "package/"
python3 .ci/scripts/windows/scan_dll.py package/imageformats/*.dll "package/"

# copy FFmpeg libraries
EXTERNALS_PATH="$(pwd)/build/externals"
FFMPEG_DLL_PATH="$(find ${EXTERNALS_PATH} -maxdepth 1 -type d | grep ffmpeg)/bin"
find ${FFMPEG_DLL_PATH} -type f -regex ".*\.dll" -exec cp -v {} package/ ';'
