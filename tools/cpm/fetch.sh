#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

[ -z "$CPM_SOURCE_CACHE" ] && CPM_SOURCE_CACHE=$PWD/.cache/cpm

mkdir -p "$CPM_SOURCE_CACHE"

# shellcheck disable=SC1091
. tools/cpm/common.sh

# shellcheck disable=SC1091
. tools/cpm/download.sh

# shellcheck disable=SC2034
ROOTDIR="$PWD"

TMP=$(mktemp -d)

usage() {
	cat << EOF
Usage: $0 [PACKAGE]...
Fetch the specified package or packages from their defined download locations.
If the package is already cached, it will not be re-fetched.

This project has defined the following as valid cpmfiles:
EOF

	for file in $CPMFILES; do
		echo "- $file"
	done

	exit 0
}

while true; do
	case "$1" in
		(-h) usage ;;
		("$0") break ;;
		("") break ;;
	esac

	PACKAGE="$1"

	shift

	export PACKAGE
	# shellcheck disable=SC1091
	. tools/cpm/package.sh

    if [ "$CI" = "true" ]; then
        ci_package
	else
		echo "-- Downloading regular package $PACKAGE, with key $KEY, from $DOWNLOAD"
	    download_package
    fi
done

rm -rf "$TMP"