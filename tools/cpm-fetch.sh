#!/bin/bash -e

# SPDX-FileCopyrightText: 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

[ -z "$CPM_SOURCE_CACHE" ] && CPM_SOURCE_CACHE=$PWD/.cache/cpm

mkdir -p $CPM_SOURCE_CACHE

ROOTDIR="$PWD"

TMP=$(mktemp -d)

download_package() {
  FILENAME=$(basename "$DOWNLOAD")

  OUTFILE="$TMP/$FILENAME"

  LOWER_PACKAGE=$(tr '[:upper:]' '[:lower:]' <<< "$PACKAGE_NAME")
  OUTDIR="${CPM_SOURCE_CACHE}/${LOWER_PACKAGE}/${KEY}"
  [ -d "$OUTDIR" ] && return

  curl "$DOWNLOAD" -sS -L -o "$OUTFILE"

  ACTUAL_HASH=$(${HASH_ALGO}sum "$OUTFILE" | cut -d" " -f1)
  [ "$ACTUAL_HASH" != "$HASH" ] && echo "$FILENAME did not match expected hash; expected $HASH but got $ACTUAL_HASH" && exit 1

  mkdir -p "$OUTDIR"

  pushd "$OUTDIR" > /dev/null

  case "$FILENAME" in
    (*.7z)
      7z x "$OUTFILE" > /dev/null
      ;;
    (*.tar*)
      tar xf "$OUTFILE" > /dev/null
      ;;
    (*.zip)
      unzip "$OUTFILE" > /dev/null
      ;;
  esac

  # basically if only one real item exists at the top we just move everything from there
  # since github and some vendors hate me
  DIRS=$(find -maxdepth 1 -type d -o -type f)

  # thanks gnu
  if [ $(wc -l <<< "$DIRS") -eq 2 ]; then
    SUBDIR=$(find . -maxdepth 1 -type d -not -name ".")
    mv "$SUBDIR"/* .
    mv "$SUBDIR"/.* . 2>/dev/null || true
    rmdir "$SUBDIR"
  fi

  if grep -e "patches" <<< "$JSON" > /dev/null; then
    PATCHES=$(jq -r '.patches | join(" ")' <<< "$JSON")
    for patch in $PATCHES; do
      patch -p1 < "$ROOTDIR"/.patch/$package/$patch
    done
  fi

  popd > /dev/null
}

ci_package() {
  REPO=$(jq -r ".repo" <<< "$JSON")
  EXT=$(jq -r '.extension' <<< "$JSON")
  [ "$EXT" == null ] && EXT="tar.zst"

  VERSION=$(jq -r ".version" <<< "$JSON")
  NAME=$(jq -r ".name | \"$package\"" <<< "$JSON")
  PACKAGE=$(jq -r ".package | \"$package\"" <<< "$JSON")

  # TODO(crueter)
  # DISABLED=$(jq -j '.disabled_platforms | join(" ")' <<< "$JSON")

  [ "$REPO" == null ] && echo "No repo defined for CI package $package" && return

  echo "CI package $PACKAGE"

  for platform in windows-amd64 windows-arm64 android solaris freebsd linux linux-aarch64; do
    FILENAME="${NAME}-${platform}-${VERSION}.${EXT}"
    DOWNLOAD="https://github.com/${REPO}/releases/download/v${VERSION}/${FILENAME}"
    PACKAGE_NAME="$PACKAGE"
    KEY=$platform

    echo "- platform $KEY"

    HASH_ALGO=$(jq -r ".hash_algo" <<< "$JSON")
    [ "$HASH_ALGO" == null ] && HASH_ALGO=sha512

    HASH_SUFFIX="${HASH_ALGO}sum"
    HASH_URL="${DOWNLOAD}.${HASH_SUFFIX}"

    HASH=$(curl "$HASH_URL" -sS -q -L -o -)

    download_package
  done
}

for package in $@
do
  # prepare for cancer
  JSON=$(find . externals src/yuzu/externals externals/ffmpeg src/dynarmic/externals externals/nx_tzdb -maxdepth 1 -name cpmfile.json -exec jq -r ".\"$package\" | select( . != null )" {} \;)

  [ -z "$JSON" ] && echo "No cpmfile definition for $package" && continue

  PACKAGE_NAME=$(jq -r ".package" <<< "$JSON")
  [ "$PACKAGE_NAME" == null ] && PACKAGE_NAME="$package"

  CI=$(jq -r ".ci" <<< "$JSON")
  if [ "$CI" != null ]; then
    ci_package
    continue
  fi

  # url parsing WOOOHOOHOHOOHOHOH
  URL=$(jq -r ".url" <<< "$JSON")
  REPO=$(jq -r ".repo" <<< "$JSON")
  SHA=$(jq -r ".sha" <<< "$JSON")

  if [ "$URL" != "null" ]; then
    DOWNLOAD="$URL"
  elif [ "$REPO" != "null" ]; then
    GIT_URL="https://github.com/$REPO"

    TAG=$(jq -r ".tag" <<< "$JSON")
    ARTIFACT=$(jq -r ".artifact" <<< "$JSON")
    BRANCH=$(jq -r ".branch" <<< "$JSON")

    if [ "$TAG" != "null" ]; then
      if [ "$ARTIFACT" != "null" ]; then
        DOWNLOAD="${GIT_URL}/releases/download/${TAG}/${ARTIFACT}"
      else
        DOWNLOAD="${GIT_URL}/archive/refs/tags/${TAG}.tar.gz"
      fi
    elif [ "$SHA" != "null" ]; then
      DOWNLOAD="${GIT_URL}/archive/${SHA}.zip"
    else
      if [ "$BRANCH" == null ]; then
        BRANCH=master
      fi

      DOWNLOAD="${GIT_URL}/archive/refs/heads/${BRANCH}.zip"
    fi
  else
    echo "No repo or URL defined for $package"
    continue
  fi

  # key parsing
  KEY=$(jq -r ".key" <<< "$JSON")

  if [ "$KEY" == null ]; then
    VERSION=$(jq -r ".version" <<< "$JSON")
    GIT_VERSION=$(jq -r ".git_version" <<< "$JSON")
    
    if [ "$SHA" != null ]; then
      KEY=$(cut -c1-4 - <<< "$SHA")
    elif [ "$GIT_VERSION" != null ]; then
      KEY="$GIT_VERSION"
    elif [ "$VERSION" != null ]; then
      KEY="$VERSION"
    else
      echo "No valid key could be determined for $package. Must define one of: key, sha, version, git_version"
      continue
    fi
  fi

  echo $KEY

  echo "Downloading regular package $package, with key $KEY, from $DOWNLOAD"

  # hash parsing
  HASH_ALGO=$(jq -r ".hash_algo" <<< "$JSON")
  [ "$HASH_ALGO" == null ] && HASH_ALGO=sha512

  HASH=$(jq -r ".hash" <<< "$JSON")

  if [ "$HASH" == null ]; then
    HASH_SUFFIX="${HASH_ALGO}sum"
    HASH_URL=$(jq -r ".hash_url" <<< "$JSON")

    if [ "$HASH_URL" == null ]; then
      HASH_URL="${DOWNLOAD}.${HASH_SUFFIX}"
    fi

    HASH=$(curl "$HASH_URL" -L -o -)
  fi

  download_package
done

rm -rf $TMP