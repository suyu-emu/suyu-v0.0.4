#! /bin/bash
set -e

# Check arguments
if [[ $# != 2 ]]; then
    >&2 echo "Bad usage!"
    echo "Usage: $0 <build dir> <output file>"
    exit 1
fi

# Get paths
ARCH="$(uname -m)"
SYSTEM_LIBS="/usr/lib"
YUZU_BIN="${1}/bin"
YUZU_BIN_GUI="${YUZU_BIN}/yuzu"

# Make sure executable exists
if [[ $(file -b --mime-type "$YUZU_BIN_GUI") != application/x-pie-executable ]]; then
    >&2 echo "Invalid yuzu executable!"
fi

# Clean up build dir
rm -rf build
mkdir build

# NOTE: some of these aren't used now, but can be reordered in priority when torzu is converted to QT6
# Find QT folder (Steam Deck fix), check for default qt5 first
QTFOUND="true"
QTDIR="$SYSTEM_LIBS"/${ARCH}-linux-gnu/qt5/plugins
if [ ! -d "$QTDIR" ]; then
  # default qt5 folder not found, check for Steam Deck qt (qt5) folder
  QTDIR="$SYSTEM_LIBS"/qt/plugins
  if [ ! -d "$QTDIR" ]; then
    # Steam Deck qt (qt5) folder not found, check for regular qt6 folder
    QTDIR="$SYSTEM_LIBS"/${ARCH}-linux-gnu/qt6/plugins
    if [ ! -d "$QTDIR" ]; then
      # regular qt6 folder not found, check for Steam Deck qt6 folder
      QTDIR="$SYSTEM_LIBS"/qt6/plugins
      if [ ! -d "$QTDIR" ]; then
        QTFOUND="false"
      fi
    fi
  fi
fi
if [ $QTFOUND == "true" ]; then
  echo "QT plugins from $QTDIR will be used."

  # Copy system dependencies used to build and required by the yuzu binary
  # includes:
  #   - '/lib64/ld-linux-x86-64.so.2' or `/lib/ld-linux-aarch64.so.1` file per architecture
  #   - required files from `/usr/lib/x86_64-linux-gnu` or `/usr/lib/aarch64-linux-gnu`
  #   - different for SteamDeck, but still does it automatically
  for lib in $(ldd "$YUZU_BIN_GUI"); do
      (cp -v "$lib" ./build/ 2> /dev/null) || true
  done

  # Copy QT dependency folders, path determined above
  cp -rv "$QTDIR"/{imageformats,platforms,platformthemes,xcbglintegrations} ./build/

  # Copy executable
  cp -v "$YUZU_BIN_GUI" ./build/

  # Copy assets for the appropriate arch
  cp -v ./assets_"${ARCH}"/* ./build/
  # Copy common assets
  cp -v ./assets/* ./build/

  # Strip all libraries and executables
  for file in $(find ./build -type f); do
      (strip -v "$file" 2> /dev/null) || true
  done

  PASSED_CHECKSUM="false"
  FILE=appimagetool.AppImage
  # total number of times to try downloading if a checksum doesn't match
  DL_TRIES=3
  while [ $PASSED_CHECKSUM == "false" ] && [ "$DL_TRIES" -gt 0 ]; do
    case $ARCH in
      x86_64)
        # Static copy from the 'ext-linux-bin' repo.
        # Checksum will need to be changed when/if this file in the repo is updated.
        if ! test -f "$FILE"; then
          echo "Downloading appimagetool for architecture '$ARCH'"
          wget -O appimagetool.AppImage https://github.com/litucks/ext-linux-bin/raw/refs/heads/main/appimage/appimagetool-x86_64.AppImage
        fi
        if [ $(shasum -a 256 appimagetool.AppImage | cut -d' ' -f1) = "110751478abece165a18460acbd7fd1398701f74a9405ad8ac053427d937bd5d" ] ; then
          PASSED_CHECKSUM="true"
        fi
        # DISABLED TO USE THE ABOVE
        # The current continuous release channel option, until a static copy is put in 'ext-linux-bin'.
        # The checksum will pass until the continuous release is updated, then a new one needs to be
        # generated to update this script.
        #if ! test -f "$FILE"; then
        #  echo "Downloading appimagetool for architecture '$ARCH'"
        #  wget -O appimagetool.AppImage https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
        #fi
        #if [ $(shasum -a 256 appimagetool.AppImage | cut -d' ' -f1) = "46fdd785094c7f6e545b61afcfb0f3d98d8eab243f644b4b17698c01d06083d1" ] ; then
        #  PASSED_CHECKSUM="true"
        #fi
        ;;
      aarch64)
        # Currently set to the continuous release channel until a static copy is put in 'ext-linux-bin'.
        # The checksum will pass until the continuous release is updated, then a new one needs to be
        # generated to update this script.
        if ! test -f "$FILE"; then
          echo "Downloading appimagetool for architecture '$ARCH'"
          wget -O appimagetool.AppImage https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-aarch64.AppImage
        fi
        if [ $(shasum -a 256 appimagetool.AppImage | cut -d' ' -f1) = "04f45ea45b5aa07bb2b071aed9dbf7a5185d3953b11b47358c1311f11ea94a96" ] ; then
          PASSED_CHECKSUM="true"
        fi
        ;;
      *)
        PASSED_CHECKSUM="invalid_arch"
        ;;
    esac
    # delete the appimagetool downloaded if the checksum doesn't match.
    if [ ! $PASSED_CHECKSUM == "true" ]; then
      rm -f appimagetool.AppImage
    fi
    ((DL_TRIES-=1))
  done
  if [ $PASSED_CHECKSUM == "true" ]; then
    echo "Checksum passed. Proceeding to build image."
    # Build AppImage
    chmod a+x appimagetool.AppImage
    ./appimagetool.AppImage ./build "$2"
  elif [ $PASSED_CHECKSUM == "invalid_arch" ]; then
    echo "No download found for architecture '$ARCH'. Building halted."
  else
    echo "Checksum for appimagetool does not match. Building halted."
    echo "If the file to be downloaded has been changed, a new checksum will need to be generated for this script."
  fi
else
  echo "QT not found, aborting AppImage build."
fi
