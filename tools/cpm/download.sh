#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# shellcheck disable=SC1091
. tools/cpm/common.sh

download_package() {
    FILENAME=$(basename "$DOWNLOAD")

    OUTFILE="$TMP/$FILENAME"

    LOWER_PACKAGE=$(echo "$PACKAGE_NAME" | tr '[:upper:]' '[:lower:]')
    OUTDIR="${CPM_SOURCE_CACHE}/${LOWER_PACKAGE}/${KEY}"
    [ -d "$OUTDIR" ] && return

    curl "$DOWNLOAD" -sS -L -o "$OUTFILE"

    ACTUAL_HASH=$("${HASH_ALGO}"sum "$OUTFILE" | cut -d" " -f1)
    [ "$ACTUAL_HASH" != "$HASH" ] && echo "!! $FILENAME did not match expected hash; expected $HASH but got $ACTUAL_HASH" && exit 1

    mkdir -p "$OUTDIR"

	PREVDIR="$PWD"
    cd "$OUTDIR"

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
    DIRS=$(find . -maxdepth 1 -type d -o -type f)

    # thanks gnu
    if [ "$(echo "$DIRS" | wc -l)" -eq 2 ]; then
        SUBDIR=$(find . -maxdepth 1 -type d -not -name ".")
        mv "$SUBDIR"/* .
        mv "$SUBDIR"/.* . 2>/dev/null || true
        rmdir "$SUBDIR"
    fi

    if echo "$JSON" | grep -e "patches" > /dev/null; then
        PATCHES=$(echo "$JSON" | jq -r '.patches | join(" ")')
        for patch in $PATCHES; do
            # shellcheck disable=SC2154
            patch --binary -p1 < "$ROOTDIR/.patch/$PACKAGE/$patch"
        done
    fi

    cd "$PREVDIR"
}

ci_package() {
    [ "$REPO" = null ] && echo "-- ! No repo defined" && return

    echo "-- CI package $PACKAGE_NAME"

	for platform in windows-amd64 windows-arm64 mingw-amd64 mingw-arm64 android solaris-amd64 freebsd-amd64 openbsd-amd64 linux-amd64 linux-aarch64 macos-universal; do
        echo "-- * platform $platform"

        case $DISABLED in
            (*"$platform"*)
                echo "-- * -- disabled"
                continue
                ;;
            (*) ;;
        esac

        FILENAME="${NAME}-${platform}-${VERSION}.${EXT}"
        DOWNLOAD="https://$GIT_HOST/${REPO}/releases/download/v${VERSION}/${FILENAME}"
        KEY=$platform

        LOWER_PACKAGE=$(echo "$PACKAGE_NAME" | tr '[:upper:]' '[:lower:]')
        OUTDIR="${CPM_SOURCE_CACHE}/${LOWER_PACKAGE}/${KEY}"
        [ -d "$OUTDIR" ] && continue

        HASH_ALGO=$(value "hash_algo")
        [ "$HASH_ALGO" = null ] && HASH_ALGO=sha512

        HASH_SUFFIX="${HASH_ALGO}sum"
        HASH_URL="${DOWNLOAD}.${HASH_SUFFIX}"

        HASH=$(curl "$HASH_URL" -sS -q -L -o -)

        download_package
    done
}
