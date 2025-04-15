#!/bin/bash -ex

set -e
declare INSTALLABLES=`find build -type d -exec test -e '{}/cmake_install.cmake' ';' -print`
set +e

GITDATE="$(git show -s --date=short --format='%ad' | sed 's/-//g')"
GITREV="$(git show -s --format='%h')"
ARTIFACTS_DIR="$PWD/artifacts"
mkdir -p "${ARTIFACTS_DIR}/"

if [ "$1" == "master" ]; then
  RELEASE_NAME="mainline"
else
  RELEASE_NAME="$1"
fi

BASE_NAME="eden-linux"
REV_NAME="${BASE_NAME}-${GITDATE}-${GITREV}"
ARCHIVE_NAME="${REV_NAME}.tar.xz"
COMPRESSION_FLAGS="-cJvf"

if [ "${RELEASE_NAME}" = "mainline" ] || [ "${RELEASE_NAME}" = "early-access" ]; then
  DIR_NAME="${BASE_NAME}-${RELEASE_NAME}"
else
  DIR_NAME="${REV_NAME}-${RELEASE_NAME}"
fi

mkdir -p "$DIR_NAME"


cp LICENSE.txt "$DIR_NAME/" || echo "LICENSE.txt not found"
cp README.md "$DIR_NAME/" || echo "README.md not found"

create_appimage() {
  local binary_name="$1"
  local display_name="$2"
  local needs_qt="$3"
  local app_dir="build/AppDir-${binary_name}"
  local appimage_output="${binary_name}.AppImage"

  echo "Creating AppImage for ${binary_name}..."

  mkdir -p "${app_dir}/usr/bin"
  mkdir -p "${app_dir}/usr/lib"
  mkdir -p "${app_dir}/usr/share/applications"
  mkdir -p "${app_dir}/usr/share/icons/hicolor/scalable/apps"
  mkdir -p "${app_dir}/usr/optional/libstdc++"
  mkdir -p "${app_dir}/usr/optional/libgcc_s"

  echo "Copying libraries..."

  for i in $INSTALLABLES; do cmake --install $i --prefix ${app_dir}/usr; done

  if [ -d "build/bin/Release" ]; then
    cp "build/bin/Release/${binary_name}" "${app_dir}/usr/bin/" || echo "${binary_name} not found for AppDir"
  else
    cp "build/bin/${binary_name}" "${app_dir}/usr/bin/" || echo "${binary_name} not found for AppDir"
  fi

  strip -s "${app_dir}/usr/bin/${binary_name}"

  cat > "${app_dir}/org.yuzu_emu.${binary_name}.desktop" << EOL
[Desktop Entry]
Type=Application
Name=${display_name}
Icon=org.yuzu_emu.${binary_name}
Exec=${binary_name}
Categories=Game;Emulator;
EOL

  cp "${app_dir}/org.yuzu_emu.${binary_name}.desktop" "${app_dir}/usr/share/applications/"

  cp "dist/eden.svg" "${app_dir}/org.yuzu_emu.${binary_name}.svg"
  cp "dist/eden.svg" "${app_dir}/usr/share/icons/hicolor/scalable/apps/org.yuzu_emu.${binary_name}.svg"

  cd build
  wget -nc https://raw.githubusercontent.com/eden-emulator/ext-linux-bin/main/appimage/deploy-linux.sh || echo "Failed to download deploy script"
  wget -nc https://raw.githubusercontent.com/eden-emulator/AppImageKit-checkrt/old/AppRun.sh || echo "Failed to download AppRun script"
  wget -nc https://github.com/eden-emulator/ext-linux-bin/raw/main/appimage/exec-x86_64.so || echo "Failed to download exec wrapper"
  chmod +x deploy-linux.sh

  cd ..
  if [ "$needs_qt" = "true" ]; then
    echo "Deploying with Qt dependencies for ${binary_name}..."
    DEPLOY_QT=1 build/deploy-linux.sh "${app_dir}/usr/bin/${binary_name}" "${app_dir}"
  else
    echo "Deploying without Qt dependencies for ${binary_name}..."
    build/deploy-linux.sh "${app_dir}/usr/bin/${binary_name}" "${app_dir}"
  fi

  cp --dereference /usr/lib/x86_64-linux-gnu/libstdc++.so.6 "${app_dir}/usr/optional/libstdc++/libstdc++.so.6" || true
  cp --dereference /lib/x86_64-linux-gnu/libgcc_s.so.1 "${app_dir}/usr/optional/libgcc_s/libgcc_s.so.1" || true

  cp build/exec-x86_64.so "${app_dir}/usr/optional/exec.so" || true
  cp build/AppRun.sh "${app_dir}/AppRun"
  chmod +x "${app_dir}/AppRun"

  find "${app_dir}" -type f -regex '.*libwayland-client\.so.*' -delete -print || true

  cd build

  if [ ! -f "appimagetool-x86_64.AppImage" ]; then
    wget -nc https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x appimagetool-x86_64.AppImage
  fi

  if ! ./appimagetool-x86_64.AppImage --version; then
    echo "FUSE not available, using extract and run mode"
    export APPIMAGE_EXTRACT_AND_RUN=1
  fi

  echo "Generating AppImage: ${appimage_output}"
  ./appimagetool-x86_64.AppImage "AppDir-${binary_name}" "${appimage_output}" || echo "AppImage generation failed for ${binary_name}"

  cp "${appimage_output}" "../${DIR_NAME}/" || echo "AppImage not copied to DIR_NAME for ${binary_name}"
  cp "${appimage_output}" "../${ARTIFACTS_DIR}/" || echo "AppImage not copied to artifacts for ${binary_name}"
  cd ..
}

create_appimage "eden" "Eden" "true"
create_appimage "eden-cli" "Eden-CLI" "false"
create_appimage "eden-room" "Eden-Room" "false"

tar $COMPRESSION_FLAGS "$ARCHIVE_NAME" "$DIR_NAME"
mv "$ARCHIVE_NAME" "${ARTIFACTS_DIR}/"

ls -la "${ARTIFACTS_DIR}/"
