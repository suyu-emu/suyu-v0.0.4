#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck disable=SC1091
. "$SCRIPTS"/../common.sh

download_package() {
	mkdir -p "$CPM_SOURCE_CACHE"

	tmp=$(make_temp_dir)

	FILENAME=$(basename "$DOWNLOAD")

	OUTFILE="$tmp/$FILENAME"

	OUTDIR="${CPM_SOURCE_CACHE}/${LOWER_PACKAGE}/${KEY}"
	if [ -d "$OUTDIR" ]; then return; fi

	curl "$DOWNLOAD" -sS -L -o "$OUTFILE"

	ACTUAL_HASH=$(sha512sum "$OUTFILE" | cut -d" " -f1)
	if [ "$ACTUAL_HASH" != "$HASH" ]; then
		echo "!! $FILENAME did not match expected hash; expected $HASH but got $ACTUAL_HASH" >&2
		exit 1
	fi

	TMPDIR="$tmp/extracted"
	mkdir -p "$OUTDIR"
	mkdir -p "$TMPDIR"

	PREVDIR="$PWD"
	mkdir -p "$TMPDIR"
	cd "$TMPDIR"

	case "$FILENAME" in
	*.7z)
		must_install 7z
		7z x "$OUTFILE" >/dev/null
		;;
	*.tar*)
		# TODO: Extensions
		must_install tar
		tar xf "$OUTFILE" >/dev/null
		;;
	*.zip)
		must_install unzip
		unzip "$OUTFILE" >/dev/null
		;;
	esac

	# basically if only one real item exists at the top we just move everything from there
	# since github and some vendors hate me
	DIRS=$(find . -maxdepth 1 -type d -o -type f)

	# thanks gnu
	if [ "$(echo "$DIRS" | wc -l)" -eq 2 ]; then
		SUBDIR=$(find . -maxdepth 1 -type d -not -name ".")
		mv "$SUBDIR"/* "$OUTDIR"
		mv "$SUBDIR"/.* "$OUTDIR" 2>/dev/null || true
		rmdir "$SUBDIR"
	else
		mv ./* "$OUTDIR"
		mv ./.* "$OUTDIR" 2>/dev/null || true
	fi

	cd "$OUTDIR"

	# TODO: Common custom patch/source cache dirs
	if echo "$JSON" | grep -e "patches" >/dev/null; then
		PATCHES=$(echo "$JSON" | jq -r '.patches | join(" ")')
		for patch in $PATCHES; do
			abs_patch="$CPMUTIL_PATCH_DIR/$PACKAGE/$patch"
			if [ ! -f "$abs_patch" ]; then
				echo "-- * Attempted to apply nonexistent patch $patch!"
				continue
			fi

			echo "-- * Applying patch $patch"
			patch --binary -p1 <"$abs_patch"
		done
	fi

	cd "$PREVDIR"

	rm -rf "$tmp"
}

# TODO: individual platform fetch?
ci_package() {
	[ "$REPO" != null ] || { echo "-- ! No repo defined" && return; }
	mkdir -p "$CPM_SOURCE_CACHE"

	echo "-- CI package $PACKAGE_NAME"

	for platform in \
		windows-amd64 windows-arm64 \
		mingw-amd64 mingw-arm64 \
		android-aarch64 android-x86_64 \
		linux-amd64 linux-aarch64 \
		macos-universal ios-aarch64; do
		echo "-- * platform $platform"

		case $DISABLED in
		*"$platform"*)
			echo "-- * -- disabled"
			continue
			;;
		esac

		FILENAME="${NAME}-${platform}-${VERSION}.${EXT}"
		DOWNLOAD="https://$GIT_HOST/${REPO}/releases/download/v${VERSION}/${FILENAME}"
		KEY="$platform-$VERSION"

		OUTDIR="${CPM_SOURCE_CACHE}/${LOWER_PACKAGE}/${KEY}"
		[ -d "$OUTDIR" ] && continue

		HASH_URL="${DOWNLOAD}.sha512sum"

		HASH=$(curl "$HASH_URL" -sS -q -L -o -)

		download_package
	done
}

fetch_package() {
	if [ "$CI" = "true" ]; then
		ci_package
	else
		echo "-- Downloading regular package $PACKAGE, with key $KEY, from $DOWNLOAD"
		download_package
	fi
}
